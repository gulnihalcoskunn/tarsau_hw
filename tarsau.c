#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_FILES 32
#define MAX_FILENAME_LENGTH 256
#define MAX_TOTAL_SIZE 200 * 1024 * 1024 // 200 MB

// Function prototypes
void createArchive(char *outputFile, char *files[], int numFiles);
void extractArchive(char *archiveFile, char *outputDir);

int main(int argc, char *argv[]) {
    if (argc < 4) {
        printf("Usage: %s -b [-o outputfile] [files...] or %s -a archivefile outputdir or %s -h for help\n", argv[0], argv[0], argv[0]);
        return 1;
    }

    char *operation = argv[1];

    if (strcmp(operation, "-b") == 0) {
        char *outputFile = "a.sau"; // Default output file name

        for (int i = 2; i < argc; i++) {
            if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
                // Use the specified output file name
                outputFile = argv[i + 1];
                i++;
            }
        }

        // Extract the input files
        char *inputFiles[MAX_FILES];
        int numInputFiles = 0;
        size_t totalSize = 0;

        for (int i = 2; i < argc; i++) {
            if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
                // Skip the -o option and its argument
                i++;
            } else {
                // Check and validate each input file
                FILE *file = fopen(argv[i], "rb");
                if (file == NULL) {
                    perror("Error opening input file");
                    exit(EXIT_FAILURE);
                }

                fseek(file, 0, SEEK_END);
                size_t fileSize = ftell(file);
                fseek(file, 0, SEEK_SET);

                if (fileSize > MAX_TOTAL_SIZE) {
                    printf("Total size of input files exceeds the limit (200 MB).\n");
                    exit(EXIT_FAILURE);
                }

                if (numInputFiles >= MAX_FILES) {
                    printf("Number of input files exceeds the limit (32).\n");
                    exit(EXIT_FAILURE);
                }

                // Check if the file contains only printable ASCII characters
                int isAscii = 1;
                int ch;
                while ((ch = fgetc(file)) != EOF) {
                    if (ch < 32 || ch > 126) {  // Printable ASCII characters range
                        isAscii = 0;
                        break;
                    }
                }

                fclose(file);

                if (!isAscii) {
                    printf("File %s format is incompatible! Only printable ASCII text files are allowed.\n", argv[i]);
                    exit(EXIT_FAILURE);
                }

                totalSize += fileSize;
                inputFiles[numInputFiles++] = argv[i];
            }
        }

        createArchive(outputFile, inputFiles, numInputFiles);
        printf("The files have been merged.\n");
    } else if (strcmp(operation, "-a") == 0) {
        char *archiveFile = argv[2];
        char *outputDir = argv[3];

        // Check if the archive file is appropriate
        FILE *archive = fopen(archiveFile, "rb");

        if (archive == NULL) {
            perror("Error opening archive file");
            exit(EXIT_FAILURE);
        }

        // Read the first 10 bytes to get the size of the organization section
        char orgSizeStr[11];
        fread(orgSizeStr, sizeof(char), 10, archive);
        orgSizeStr[10] = '\0';
        size_t orgSize = atoi(orgSizeStr);

        fseek(archive, orgSize, SEEK_SET);

        // Your code to check the appropriateness or corruption of the archive file goes here
        // ...

        fclose(archive);

        // Extract the contents of the archive
        extractArchive(archiveFile, outputDir);
        printf("Files opened in the %s directory.\n", outputDir);
    } else {
        printf("Invalid operation. Use -b to create an archive or -a to extract from an archive.\n");
        return 1;
    }

    return 0;
}

void createArchive(char *outputFile, char *files[], int numFiles) {
    FILE *archive = fopen(outputFile, "wb");

    if (archive == NULL) {
        perror("Error opening output file");
        exit(EXIT_FAILURE);
    }

    // Write the initial placeholder for the organization section size
    char orgSizePlaceholder[] = "0000000000";
    fwrite(orgSizePlaceholder, sizeof(char), 10, archive);

    // Write the organization (contents) section
    for (int i = 0; i < numFiles; i++) {
        FILE *file = fopen(files[i], "rb");

        if (file == NULL) {
            perror("Error opening input file");
            exit(EXIT_FAILURE);
        }

        // Write the filename, permissions, and size to the organization section
        fprintf(archive, "%s|%s|%ld|", files[i], "rw-r--r--", ftell(file));

        fclose(file);
    }

    // Get the current position (end of organization section) and update the organization size
    size_t orgSize = ftell(archive);
    fseek(archive, 0, SEEK_SET);
    fprintf(archive, "%010lu", orgSize);

    // Move to the end of the organization section
    fseek(archive, orgSize, SEEK_SET);

    // Write the archived files
    for (int i = 0; i < numFiles; i++) {
        FILE *file = fopen(files[i], "rb");

        if (file == NULL) {
            perror("Error opening input file");
            exit(EXIT_FAILURE);
        }

                char buffer[1024];
        size_t bytesRead;

        while ((bytesRead = fread(buffer, sizeof(char), sizeof(buffer), file)) > 0) {
            fwrite(buffer, sizeof(char), bytesRead, archive);
        }

        fclose(file);
    }

    fclose(archive);
}

void extractArchive(char *archiveFile, char *outputDir) {
    fprintf(stderr, "Starting extraction...\n");
    FILE *archive = fopen(archiveFile, "rb");

    if (archive == NULL) {
        perror("Error opening archive file");
        exit(EXIT_FAILURE);
    }

    // Read the first 10 bytes to get the size of the organization section
    char orgSizeStr[11];
    fread(orgSizeStr, sizeof(char), 10, archive);
    orgSizeStr[10] = '\0';
    size_t orgSize = atoi(orgSizeStr);

    fprintf(stderr, "Reading organization section...\n");
    fprintf(stderr, "Organization size: %lu\n", orgSize);

    // Read the organization section
    char *orgSection = (char *)malloc(orgSize + 1);
    if (orgSection == NULL) {
        perror("Error allocating memory");
        exit(EXIT_FAILURE);
    }
    fprintf(stderr, "Organization section: %s\n", orgSection);

    fread(orgSection, sizeof(char), orgSize, archive);
    orgSection[orgSize] = '\0';

    // Parse the organization section and extract file information
    char *record = strtok(orgSection, "|");
    while (record != NULL) {
        char *fileName = strtok(NULL, ",");
        char *permissions = strtok(NULL, ",");
        char *sizeStr = strtok(NULL, "|");

        if (fileName != NULL && permissions != NULL && sizeStr != NULL) {
            size_t size = atol(sizeStr);

            // Construct the output file path
            char outputPath[MAX_FILENAME_LENGTH];
            snprintf(outputPath, sizeof(outputPath), "%s/%s", outputDir, fileName);

            // Create the output directory if it doesn't exist
            struct stat st = {0};
            if (stat(outputDir, &st) == -1) {
                if (mkdir(outputDir, 0700) == -1) {
                    perror("Error creating output directory");
                    exit(EXIT_FAILURE);
                }
            }

            FILE *outputFile = fopen(outputPath, "wb");

            if (outputFile == NULL) {
                perror("Error creating output file");
                exit(EXIT_FAILURE);
            }

            // Read the file contents from the archive and write to the output file
            char buffer[1024];
            size_t bytesRead;

            while ((bytesRead = fread(buffer, sizeof(char), sizeof(buffer), archive)) > 0) {
                fwrite(buffer, sizeof(char), bytesRead, outputFile);
                size -= bytesRead;

                if (size <= 0) {
                    break;
                }
            }

            fclose(outputFile);

            // Set the file permissions based on the original permissions
            chmod(outputPath, strtol(permissions, NULL, 8));
        }

        record = strtok(NULL, "|");
    }

    fclose(archive);
    fprintf(stderr, "Extraction complete.\n");
    free(orgSection);
}

