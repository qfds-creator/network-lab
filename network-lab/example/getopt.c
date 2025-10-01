#include <stdio.h>
#include <unistd.h>

void usage(const char *progname) {
  fprintf(stderr, "Usage: %s [-h] [-X data] <URL>\n", progname);
  fprintf(stderr, "Options:\n");
  fprintf(stderr, "  -h          Show this help message\n");
  fprintf(stderr, "  -X data     Option X with data\n");
  fprintf(stderr, "  -i          Option I\n");
}

int main(int argc, char *argv[]) {
  int opt;
  char *X_data = NULL;
  int i_flag = 0;

  // h and i are flags; X expect arguments
  while ((opt = getopt(argc, argv, "hX:i")) != -1) {
    switch (opt) {
    case 'h':
      usage(argv[0]);
      return 0;
    case 'X':
      X_data = optarg;
      break;
    case 'i':
      i_flag = 1;
      break;
    default:
      usage(argv[0]);
      return 1;
    }
  }

  if (optind != argc - 1) {
    fprintf(stderr, "Error: Must provide exactly one <URL> argument.\n");
    usage(argv[0]);
    return 1;
  }

  char *url = argv[optind];

  printf("Options:\n");
  if (X_data)
    printf("  -X data: %s\n", X_data);
  printf("  -i %s\n", i_flag ? "enabled" : "disabled");
  printf("URL: %s\n", url);

  return 0;
}
