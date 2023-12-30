#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>
#include <ctype.h>

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

void writeToArchive(FileInfo *fileInfos, int numFiles, const char *outputFileName);

void processFile(FileInfo *fileInfos, int *numFiles, long *totalSize, const char *filename);

void extractArchive(const char *archiveFileName,const char *extractDirectory);

bool isBinary(FILE *file);

void handleFileError(const char *action, const char *filename);


int main(int argc, char *argv[]) {
    long totalSize=0;
    char *outputFileName = "a.sau";  // Default output file name

    if (argc < 3 || (strcmp(argv[1], "-b") != 0 && strcmp(argv[1], "-a") != 0)) {
         printf("Usage: %s -b input_files -o output_file\n", argv[0]);
        printf("       %s -a archive_file extract_directory\n", argv[0]);
        return EXIT_FAILURE;

    } else if (strcmp(argv[1], "-b") == 0) {
        FileInfo fileInfos[MAX_FILES];
        int numFiles = 0;

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
                printf("Archive file is inappropriate or corrupt!\n");
                freeFileInfoContent(fileInfos, numFiles);
                return EXIT_FAILURE;
            }
            outputFileName = argv[outputIndex + 1];
        } else {
            printf("Output file name not provided, using default 'a.sau'.\n");
        }

        writeToArchive(fileInfos, numFiles,outputFileName);
    } else if (strcmp(argv[1], "-a") == 0) {
        if (argc < 4) {
            printf("Usage: %s -a archive_file extract_directory [-o output_file]\n", argv[0]);
            return EXIT_FAILURE;
        }

        char *archiveFileName = argv[2];
        char *extractDirectory = argc == 4 ? argv[3] : ".";

        FileInfo fileInfos[MAX_FILES];
        int numFiles = 0;

        // Read the archive file to populate fileInfos with content
        extractArchive(archiveFileName,extractDirectory);
        
        // Check if output file is specified
        int outputIndex = -1;
        for (int i = 4; i < argc; i++) {
            if (strcmp(argv[i], "-o") == 0) {
                outputIndex = i;
                break;
            }
        }

        if (outputIndex != -1) {
            if (outputIndex + 1 >= argc || !strstr(argv[outputIndex + 1], ".sau")) {
                printf("Archive file is inappropriate or corrupt!\n");
                freeFileInfoContent(fileInfos, numFiles);
                return EXIT_FAILURE;
            }
            outputFileName = argv[outputIndex + 1];
        } 

       // writeToArchive(fileInfos, numFiles,outputFileName);
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

void writeToArchive(FileInfo *fileInfos, int numFiles, const char *outputFileName) {
    FILE *archiveFile = fopen(outputFileName, "wb");
    if (!archiveFile) {
        printf("Error creating archive file!\n");
        freeFileInfoContent(fileInfos, numFiles);
        exit(EXIT_FAILURE);
    }

    // Calculate total size
    long totalSize = 0;
    for (int i = 0; i < numFiles; i++) {
        totalSize += fileInfos[i].size;
    }

    // Write the Organization Section header
    fprintf(archiveFile, "Size: %010ld|", totalSize);

    // Write the content of each archived file
    for (int i = 0; i < numFiles; i++) {
        fprintf(archiveFile, "%s,%s,%ld", fileInfos[i].filename, fileInfos[i].permissions, fileInfos[i].size);

        // Check if it's not the last file, then print a separator
        if (i < numFiles - 1) {
            fprintf(archiveFile, "|");
        }
    }

    // Write a newline character to separate the headers and content
    fprintf(archiveFile, "\n");

    // Write the Organization Section contents
    for (int i = 0; i < numFiles; i++) {
        FILE *file = fopen(fileInfos[i].filename, "rb");
        if (!file) {
            printf("Error opening file %s!\n", fileInfos[i].filename);
            fclose(archiveFile);
            freeFileInfoContent(fileInfos, numFiles);
            exit(EXIT_FAILURE);
        }

        // Read the content of the file
        char *fileContent = NULL;
        fseek(file, 0, SEEK_END);
        long fileSize = ftell(file);
        rewind(file);

        if (fileSize > 0) {
            fileContent = (char *)malloc(fileSize + 1);
            if (!fileContent) {
                perror("Memory allocation error");
                fclose(file);
                fclose(archiveFile);
                freeFileInfoContent(fileInfos, numFiles);
                exit(EXIT_FAILURE);
            }

            fread(fileContent, sizeof(char), fileSize, file);
            fileContent[fileSize] = '\0';  // Null-terminate the content
        }

        fclose(file);

        // Write the entire content to the archive file without adding newlines
        fprintf(archiveFile, "%s", fileContent);

        free(fileContent);
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


void extractArchive(const char *archiveFileName, const char *extractDirectory) {
    FILE *archiveFile = fopen(archiveFileName, "rb");
    if (!archiveFile) {
        handleFileError("opening archive file", archiveFileName);
    }

    // Create the target directory if it doesn't exist
    struct stat st = {0};
    if (stat(extractDirectory, &st) == -1) {
        if (mkdir(extractDirectory, 0755) == -1) {
            perror("Error creating directory");
            fclose(archiveFile);
            exit(EXIT_FAILURE);
        }
    }

    if (chdir(extractDirectory) == -1) {
        perror("Error changing directory");
        fclose(archiveFile);
        exit(EXIT_FAILURE);
    }

    char buffer[LINE_BUFFER_SIZE];

    // Read the Organization Section header and size
    if (fgets(buffer, sizeof(buffer), archiveFile) == NULL || strncmp(buffer, "Size: ", 6) != 0) {
        fprintf(stderr, "Invalid archive file format (missing Size header).\n");
        fclose(archiveFile);
        exit(EXIT_FAILURE);
    }
    // Extract total size
    long totalSize = strtol(buffer + 6, NULL, 10);

    // Tokenize the Organization Section contents
    char *token = strtok(buffer, "|");

    while (token != NULL) {
        // Skip empty tokens
        if (strlen(token) > 0) {
            char filePath[FILEPATH_BUFFER_SIZE];
            char permissions[10];
            size_t fileSize;

            // Tokenize each file entry
            int result = sscanf(token, "%[^,],%[^,],%lu", filePath, permissions, &fileSize);

            if (result == 3) {
                FILE *outputFile = fopen(filePath, "w");  // Change mode from "wb" to "w"
                if (!outputFile) {
                    handleFileError("creating file", filePath);
                }

                // Allocate memory for content
                char *fileContent = (char *)malloc(fileSize);
                if (!fileContent) {
                    perror("Memory allocation error");
                    fclose(outputFile);
                    fclose(archiveFile);
                    exit(EXIT_FAILURE);
                }

                // Read the content of the file
                size_t readSize = fread(fileContent, sizeof(char), fileSize, archiveFile);

                // Write the entire content to the file
                fwrite(fileContent, sizeof(char), fileSize, outputFile);

                fclose(outputFile);
                free(fileContent);

                printf("%s,",filePath);
            }
        }

        // Get the next token
        token = strtok(NULL, "|");
    }

    fclose(archiveFile);

    printf("files opened in the %s directory.\n", extractDirectory);
}

