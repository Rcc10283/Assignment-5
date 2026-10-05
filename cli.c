#include "kernel.h"
#include <string.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
int generate_pagefault() {
    const char* filename = "pagefault.bin";
    size_t size = 1024 * 1024;

    int fd = open(
        filename,
        O_RDWR | O_CREAT | O_TRUNC,
        0644
    );

    if (fd == -1) {
        perror("open");
        return -1;
    }

    if (ftruncate(fd, size) == -1) {
        perror("ftruncate");
        close(fd);
        return -1;
    }

    char* mapping = mmap(
        NULL,
        size,
        PROT_READ | PROT_WRITE,
        MAP_SHARED,
        fd,
        0
    );

    if (mapping == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return -1;
    }

    /*
     * Write data into the mapping and force it to disk.
     */
    for (size_t i = 0; i < size; i += 4096) {
        mapping[i] = 1;
    }

    msync(mapping, size, MS_SYNC);

    /*
     * Tell the kernel we no longer need these pages.
     */
    madvise(mapping, size, MADV_DONTNEED);

    /*
     * Access the pages again.
     * volatile prevents the compiler from optimizing the read away.
     */
    volatile char value = 0;

    for (size_t i = 0; i < size; i += 4096) {
        value += mapping[i];
    }

    (void)value;

    munmap(mapping, size);
    close(fd);
    unlink(filename);

    return 0;
}

int main(int argc, char** argv){
    if(argc != 6) {
        printf("Incorrect number of arguments. Expected: ./cli <MODE=kernel|mmap|convert|uconvert|fault> <input_image> <width> <height> <output_image_path>\n");
        return -1;
    }

    char* mode = argv[1];
    char* filepath = argv[2];
    int width = atoi(argv[3]);
    int height = atoi(argv[4]);
    char* output_filepath = argv[5];

    if(strcmp(mode, "kernel") == 0) {

        struct image img;
        img.width = width;
        img.height = height;

        if(loadimage(filepath, &img) != 0) {
            printf("Failed to load image\n");
            return -1;
        }

        int kernel[3][3] = {
            {1, 1, 1},
            {1, 1, 1},
            {1, 1, 1}
        };

        struct image* output = apply_kernel(
            &img,
            (int*)kernel,
            3,
            1.0f / 9.0f
        );

        if(saveimage(output_filepath, output) != 0) {
            printf("Failed to save image\n");

            free(img.pixels);
            free(output->pixels);
            free(output);

            return -1;
        }

        free(img.pixels);
        free(output->pixels);
        free(output);

        return 0;
    }
        if(strcmp(mode, "convert") == 0) {
        struct image img;
        img.width = width;
        img.height = height;

        if(loadimage(filepath, &img) != 0) {
            printf("Failed to load image\n");
            return -1;
        }

        if(saveimage_mmap(output_filepath, &img) != 0) {
            printf("Failed to save mmap image\n");
            free(img.pixels);
            return -1;
        }

        free(img.pixels);
        return 0;
    }

    if(strcmp(mode, "uconvert") == 0) {
        struct image img;
        img.width = width;
        img.height = height;

        if(loadimage_mmap(filepath, &img) != 0) {
            printf("Failed to load mmap image\n");
            return -1;
        }

        size_t mapping_size =
            sizeof(struct image) +
            width * height * sizeof(struct pixel);

        void* mapping =
            (char*)img.pixels - sizeof(struct image);

        if(saveimage(output_filepath, &img) != 0) {
            printf("Failed to save BMP image\n");
            munmap(mapping, mapping_size);
            return -1;
        }

        munmap(mapping, mapping_size);
        return 0;
    }

    if(strcmp(mode, "mmap") == 0) {
        struct image img;
        img.width = width;
        img.height = height;

        if(loadimage_mmap(filepath, &img) != 0) {
            printf("Failed to load mmap image\n");
            return -1;
        }

        size_t mapping_size =
            sizeof(struct image) +
            width * height * sizeof(struct pixel);

        void* mapping =
            (char*)img.pixels - sizeof(struct image);

        int kernel[3][3] = {
            {1, 1, 1},
            {1, 1, 1},
            {1, 1, 1}
        };

        struct image* output = apply_kernel(
            &img,
            (int*)kernel,
            3,
            1.0f / 9.0f
        );

        if(saveimage(output_filepath, output) != 0) {
            printf("Failed to save image\n");

            munmap(mapping, mapping_size);
            free(output->pixels);
            free(output);

            return -1;
        }

        munmap(mapping, mapping_size);
        free(output->pixels);
        free(output);

        return 0;
    }
    if(strcmp(mode, "fault") == 0) {
        return generate_pagefault();
    }
    printf("Mode not implemented yet: %s\n", mode);
    return -1;
}