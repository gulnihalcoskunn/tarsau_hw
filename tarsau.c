#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdbool.h>

#define MAX_FILES 32
#define MAX_SIZE (200 * 1024 * 1024) // 200 MB
#define LINE_BUFFER_SIZE 1000
#define FILENAME_BUFFER_SIZE 256
#define CONTENT_BUFFER_SIZE 512
#define FILEPATH_BUFFER_SIZE 512

typedef struct {
    char filename[FILENAME_BUFFER_SIZE];
    char permissions[10];
    size_t size;
    char *content;  // Dynamic buffer to store file content
} FileInfo;

void freeFileInfoContent(FileInfo *fileInfos, int numFiles);

void writeToArchive(FileInfo *fileInfos, int numFiles, long totalSize, const char *outputFileName);

void processFile(FileInfo *fileInfos, int *numFiles, long *totalSize, const char *filename);

void readArchive(const char *archiveFileName, FileInfo *fileInfos, int *numFiles);

void extractArchive(const char *archiveFileName, FileInfo *fileInfos, int numFiles, const char *extractDirectory);

bool isBinary(FILE *file);

void handleFileError(const char *action, const char *filename);


int main(int argc, char *argv[]) {
    if (argc < 3 || (strcmp(argv[1], "-b") != 0 && strcmp(argv[1], "-a") != 0)) {
        printf("Usage: %s -b input_files -o output_file\n", argv[0]);
        printf("       %s -a archive_file extract_directory\n", argv[0]);
        return EXIT_FAILURE;
    } else if (strcmp(argv[1], "-b") == 0) {
        char *outputFileName = "a.sau"; // Default output file name
        FileInfo fileInfos[MAX_FILES];
        int numFiles = 0;
        long totalSize = 0;

        int outputIndex = -1;
        for (int i = 2; i < argc; i++) {
            if (strcmp(argv[i], "-o") == 0) {
                outputIndex = i;
                break;
            } else {
                processFile(fileInfos, &numFiles, &totalSize, argv[i]);
            }
        }
        if (outputIndex != -1) {
            if (outputIndex + 1 >= argc || !strstr(argv[outputIndex + 1], ".sau")) {
                printf("Invalid output file name!\n");
                freeFileInfoContent(fileInfos, numFiles);
                return EXIT_FAILURE;
            }
            outputFileName = argv[outputIndex + 1];
        } else {
            printf("Output file name not provided, using default 'a.sau'.\n");
        }

        writeToArchive(fileInfos, numFiles, totalSize, outputFileName);
    } else if (strcmp(argv[1], "-a") == 0) {
        if (argc < 4) {
            printf("Usage: %s -a archive_file extract_directory\n", argv[0]);
            return EXIT_FAILURE;
        }

        char *archiveFileName = argv[2];
        char *extractDirectory = argc == 4 ? argv[3] : ".";

        FileInfo fileInfos[MAX_FILES];
        int numFiles = 0;

        //readArchive(archiveFileName, fileInfos, &numFiles);
        extractArchive(archiveFileName,fileInfos, numFiles, extractDirectory);
    }

    return EXIT_SUCCESS;
}

void handleFileError(const char *action, const char *filename) {
    fprintf(stderr, "Error %s file: %s\n", action, filename);
    perror(NULL);
    exit(EXIT_FAILURE);
}

void freeFileInfoContent(FileInfo *fileInfos, int numFiles) {
    for (int i = 0; i < numFiles; i++) {
        free(fileInfos[i].content);
    }
}

void writeToArchive(FileInfo *fileInfos, int numFiles, long totalSize, const char *outputFileName) {
    FILE *archiveFile = fopen(outputFileName, "wb");
    if (archiveFile == NULL) {
        printf("Error creating archive file!\n");
        freeFileInfoContent(fileInfos, numFiles);
        exit(EXIT_FAILURE);
    }

    // Write the Organization Section header
    fprintf(archiveFile, "Organization Section:\n");
    fprintf(archiveFile, "Size: %010ld\n", totalSize);

    // Write the Organization Section contents
    fprintf(archiveFile, "|");
    for (int i = 0; i < numFiles; i++) {
        fprintf(archiveFile, "%s,%s,%ld|", fileInfos[i].filename, fileInfos[i].permissions, fileInfos[i].size);
    }
    fprintf(archiveFile, "\n");

    // Write the Archived Files header
    fprintf(archiveFile, "Archived Files:\n");

    // Write the content of each archived file
    for (int i = 0; i < numFiles; i++) {
        fprintf(archiveFile, "im %s\n", fileInfos[i].filename);
    }

    fclose(archiveFile);

    printf("The files have been merged.\n");
}


bool isBinary(FILE *file) {
    int ch;
    while ((ch = fgetc(file)) != EOF) {
        if (ch == 0) {
            return true;  // Null byte found, indicating a binary file
        }
    }
    return false;  // No null bytes found, indicating a text file
}

void processFile(FileInfo *fileInfos, int *numFiles, long *totalSize, const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (file) {
        fseek(file, 0, SEEK_END);
        long fileSize = ftell(file);
        rewind(file);

        // Check if the file is binary
        if (isBinary(file)) {
            printf("%s input file format is incompatible! \n", filename);
            fclose(file);
            return;
        }

        // Obtain file permissions
        struct stat fileStat;
        if (stat(filename, &fileStat) == 0) {
            mode_t permissions = fileStat.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO);
            snprintf(fileInfos[*numFiles].permissions, sizeof(fileInfos[*numFiles].permissions), "%o", permissions);
        } else {
            perror("Failed to obtain permissions for file");
            snprintf(fileInfos[*numFiles].permissions, sizeof(fileInfos[*numFiles].permissions), "default");
        }

        strcpy(fileInfos[*numFiles].filename, filename);
        fileInfos[*numFiles].size = fileSize;

        // Allocate memory for content and read file content
        fileInfos[*numFiles].content = (char *)malloc(fileSize + 1);
        if (fileInfos[*numFiles].content == NULL) {
            perror("Memory allocation error");
            fclose(file);
            return;
        }

        fread(fileInfos[*numFiles].content, sizeof(char), fileSize, file);
        fileInfos[*numFiles].content[fileSize] = '\0';  // Null-terminate the content
        *totalSize += fileSize;
        (*numFiles)++;

        fclose(file);
    } else {
        perror("Error opening file");
    }
}

void extractArchive(const char *archiveFileName, FileInfo *fileInfos, int numFiles, const char *extractDirectory) {
    FILE *archiveFile = fopen(archiveFileName, "rb");
    if (!archiveFile) {
        perror("Error opening archive file");
        exit(EXIT_FAILURE);
    }

    size_t organizationSize;
    fscanf(archiveFile, "%010zu", &organizationSize);
    fseek(archiveFile, organizationSize, SEEK_SET);

    struct stat st = {0};
    if (stat(extractDirectory, &st) == -1) {
        if (mkdir(extractDirectory, 0755) == -1) {
            perror("Error creating directory");
            freeFileInfoContent(fileInfos, numFiles);
            exit(EXIT_FAILURE);
        }
    }

    char buffer[1024];
    fgets(buffer, sizeof(buffer), archiveFile);

    char *token = strtok(buffer, "|");
    while (token != NULL) {
        FileInfo fileInfo;
        int result = sscanf(token, "%[^,],%[^,],%zu", fileInfo.filename, fileInfo.permissions, &fileInfo.size);

        // Check for valid tokenization and skip empty filenames
        if (result == 3 && fileInfo.filename[0] != '\0') {
            // Print file information for debugging
            printf("File: %s, Permissions: %s, Size: %zu\n", fileInfo.filename, fileInfo.permissions, fileInfo.size);

            // Create the full path for the extracted file
            char filePath[FILEPATH_BUFFER_SIZE];
            snprintf(filePath, sizeof(filePath), "%s/%s", extractDirectory, fileInfo.filename);

            FILE *outputFile = fopen(filePath, "wb");
            if (!outputFile) {
                perror("Error creating file");
                freeFileInfoContent(fileInfos, numFiles);
                exit(EXIT_FAILURE);
            }

            // Copy content from the archive to the extracted file
            size_t bytesRead = 0;
            while (bytesRead < fileInfo.size) {
                char buffer[CONTENT_BUFFER_SIZE];
                size_t chunkSize = fread(buffer, 1, sizeof(buffer), archiveFile);
                fwrite(buffer, 1, chunkSize, outputFile);
                bytesRead += chunkSize;
            }

            fclose(outputFile);

            // Set file permissions
            mode_t permissions;
            sscanf(fileInfo.permissions, "%o", &permissions);
            if (chmod(filePath, permissions) == -1) {
                perror("Error setting file permissions");
                freeFileInfoContent(fileInfos, numFiles);
                exit(EXIT_FAILURE);
            }
        }

        token = strtok(NULL, "|");
    }

    fclose(archiveFile);

    printf("Files extracted to the %s directory.\n", extractDirectory);
}


