#include <bfd.h>
#include <iostream>

int main(int argc, char **argv) {
  if (argc == 1) {
    std::cerr << "Lack input file!\n";
    return 1;
  }

  std::cout << "Input: " << argv[1] << '\n';

  bfd_init();

  if (auto file = bfd_openr(argv[1], nullptr)) {
    if (bfd_check_format(file, bfd_archive)) {
      std::cout << argv[1] << " contains following files:\n";
      bfd *last_arfile = nullptr;

      while (bfd *arfile = bfd_openr_next_archived_file(file, arfile)) {
        if (arfile) {
          std::cout << bfd_get_filename(arfile) << '\n';
        } else {
          if (bfd_get_error() != bfd_error_no_more_archived_files) {
            bfd_perror("bfd_openr_next_archived_file");
            return 1;
          }
          break;
        }

        if (last_arfile) {
          bfd_close(last_arfile);
          last_arfile = nullptr;
          if (last_arfile == arfile) {
            break;
          }
        }
        last_arfile = arfile;
      }

      if (last_arfile) {
        bfd_close(last_arfile);
      }
    } else {
      std::cout << argv[1] << " is not archive!\n";
      return 1;
    }

    bfd_close(file);
  } else {
    bfd_perror("Open file failed");
    return 1;
  }

  return 0;
}
