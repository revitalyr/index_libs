#include <bfd.h>
#include <iostream>
#include <sstream>
#include <string_view>

struct BfdWrapper
{
    bfd     *m_bfd{ nullptr };

    BfdWrapper() = default;

    explicit BfdWrapper(std::string_view filename) {
        static bool init = [](){
            bfd_init();
            return true;
        } ();

        if( !(m_bfd = bfd_openr(filename.data(), nullptr)) ) {
            throw std::runtime_error((std::stringstream() << "Could not open '" << filename << "': " << bfd_errmsg(bfd_get_error())).str());
        }
    }

    explicit BfdWrapper(bfd *bfd) : m_bfd{ bfd } {
        if(!(bfd_check_format (m_bfd, bfd_object) || bfd_check_format (m_bfd, bfd_archive))) {
            throw std::runtime_error("Bad source bfd");
        }
    }

    ~BfdWrapper() noexcept {
        close();
    }

    BfdWrapper & operator=(BfdWrapper &&rlh) noexcept {
        close();
        m_bfd = rlh.m_bfd;
        rlh.m_bfd = nullptr;
        return *this;
    }

    operator bool() const noexcept {
        return m_bfd != nullptr;
    }

    BfdWrapper  open_next_archive(BfdWrapper const &previous) noexcept {
        if (auto next_arc{bfd_openr_next_archived_file(m_bfd, previous.m_bfd)}; next_arc != nullptr) {
            return BfdWrapper{ next_arc };
        }
        return BfdWrapper{ };
    }

    void close() noexcept {
        if(*this) {
            bfd_close(m_bfd);
            m_bfd = nullptr;
        }
    }

    bool is_format(bfd_format format) const noexcept {
        return bfd_check_format (m_bfd, format);
    }

    const char *filename() const noexcept {
        return m_bfd ? m_bfd->filename : "";
    }
};
 
int main(int argc, char **argv) {
  if (argc == 1) {
    std::cerr << "Lack input file!\n";
    return 1;
  }

  try{
    std::cout << "Input: " << argv[1] << '\n';
    auto file{ BfdWrapper(argv[1]) };

    if (file.is_format(bfd_archive)) {
        BfdWrapper  arfile;

        std::cout << file.filename() << " contains following files:\n";

        while ( (arfile = file.open_next_archive(arfile)) != false) {
            std::cout << arfile.filename() << '\n';
        }
    } else {
        std::cerr << file.filename() << " is not archive!\n";
        return 1;
    }
  } 
  catch(std::exception const & ex) {
    std::cerr << typeid(ex).name() << ": " << ex.what() << '\n';
    return 1;
  }

  return 0;
}
