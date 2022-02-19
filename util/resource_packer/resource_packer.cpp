#include <stdio.h>
#include <stdlib.h>

static const int kEntriesPerLine = 16;

static int generate_resource_file(const char *input_filename,
                                  const char *output_filename,
                                  const char *variable_name) {
  unsigned char buf[512];
  bool first_byte = true;
  FILE *input = NULL;
  FILE *output = NULL;

  input = fopen(input_filename, "rb");
  if (!input) {
    perror(input_filename);
    return EXIT_FAILURE;
  }

  output = fopen(output_filename, "w");
  if (!output) {
    perror(output_filename);
    fclose(input);
    return EXIT_FAILURE;
  }

  fprintf(output, "/* Auto generated from %s */\n\n", input_filename);
  fprintf(output, "static const unsigned char %s[] = {\n", variable_name);

  while (!feof(input)) {
    size_t num_read = fread(buf, 1, sizeof(buf), input);
    if (!num_read) {
      break;
    }

    for (size_t i = 0; i < num_read; ++i) {
      if (!(i % kEntriesPerLine)) {
        fprintf(output, "\n  ");
      }
      if (first_byte) {
        fprintf(output, "  0x%02X", buf[i]);
        first_byte = false;
      } else {
        fprintf(output, " , 0x%02X", buf[i]);
      }
    }
  }

  if (ferror(input)) {
    fprintf(stderr, "Failed to read from %s\n", input_filename);
    fclose(output);
    fclose(input);
    return EXIT_FAILURE;
  }

  fprintf(output, "\n};\n");
  fclose(output);
  fclose(input);

  return EXIT_SUCCESS;
}

int main(int argc, char **argv) {
  if (argc < 4) {
    fprintf(stderr,
            "Usage: %s {input_filename} {output_filename} {variable_name}\n",
            argv[0]);
    return EXIT_FAILURE;
  }

  return generate_resource_file(argv[1], argv[2], argv[3]);
}