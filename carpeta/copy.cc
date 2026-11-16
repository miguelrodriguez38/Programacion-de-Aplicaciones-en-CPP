#include <filesystem>
#include <cstdlib>
#include <print>
#include <string>
#include <sys/stat.h>
#include <expected>
#include <system_error>
#include <fcntl.h>    // open() y sus flags
#include <unistd.h>   // read(), write(), close()
#include <format>     // std::format
#include <cerrno>     // errno
#include <array>


bool check_args(int argc, char* argv[]) {
    if (argc != 3)
    {
        std::println(stderr, "copy: se deben indicar los archivos ORIGEN y DESTINO");
        return false;
    }
    return true;

}

bool is_directory(const std::string& path) {
    struct stat st;
    if(stat(path.c_str(), &st) == -1) {
        return false;
    } else {
        if (S_ISDIR(st.st_mode)) {
            return true;
        } else {
            return false;
        }
    }
}

bool is_same_file(const std::string& src_path, const std::string& dest_path) {
    struct stat st_src;
    struct stat st_dest;
    if(stat(src_path.c_str(), &st_src) == -1) {
        return false;
    } 
    if(stat(dest_path.c_str(), &st_dest) == -1) {
        return false;
    }
    if ( st_src.st_dev == st_dest.st_dev && st_src.st_ino == st_dest.st_ino ) {
        return true;
    }
    return false;
}



std::unexpected<std::system_error> system_error_from_errno(std::string context) {
    return std::unexpected(std::system_error(errno, std::system_category(), context));
}


std::expected<void, std::system_error> copy_file(const std::string& src_path, const std::string& dest_path, mode_t dst_perms = 0644) { //el 0644 es un valor por defecto , son los permisos con los que se creará eñl archivo
    int src_fd = open(src_path.c_str(), O_RDONLY);  //abrir el origen con O_RDONLY que es solo lectura
    if (src_fd == -1) {
        return system_error_from_errno("Error al abrir el archivo de origen");
    }

    int dest_fd = open(dest_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, dst_perms);  //abrir el destino con O_WRONLY | O_CREAT | O_TRUNC, dst_perms, que es Write only, si no existe crealo con los permisos dst_perms y si existe pues sobreescribe
    if (dest_fd == -1) {
        auto error = system_error_from_errno("Error al abrir el archivo de destino");
        close(src_fd); // cerramos el origen también, se exige cerrarlo todo
        return error; // se devuelve el error creado con la funcion system_error_from_errno del tipo std::system_error
    }

    constexpr std::size_t BLOCK_SIZE = 128 * 1024;
    std::array<char, BLOCK_SIZE> buffer;

    while (true) {
        ssize_t bytes_read = read(src_fd, buffer.data(), buffer.size());
        if (bytes_read == -1) {
            auto error = system_error_from_errno("Error al leer del archivo de origen");
            close(dest_fd); //Se cierran ambos archivos
            close(src_fd);
            return error;
        }
        if (bytes_read == 0) {
            break;   // fin de archivo
        }

        ssize_t total_written = 0;
        while (total_written < bytes_read) {  // llevo x writes escritos
            ssize_t bytes_written = write(dest_fd, buffer.data() + total_written, bytes_read - total_written); // write no garantiza escribirlos todos de golpe
            if (bytes_written == -1) {
                auto error = system_error_from_errno("Error al escribir en el archivo de destino");
                close(dest_fd);
                close(src_fd);
                return error;
                //buffer.data() + total_written --> Desde que parte arranco del bufer
                //bytes_read - total_written --> Cuantos bytes pedir que son los que hacen falta
            }
            total_written += bytes_written; // apuntas lo que write ha escrito 
            //write es lazy es decir no hace todo
        }
    }
   

    close(dest_fd); // cierras ambos archivos
    close(src_fd);
    return {}; // significa todo ha ido , void sin nada dentro
}

int main (int argc, char* argv[]) {
    if (!check_args(argc, argv))
    {
        return EXIT_FAILURE;
    }
    std::string src_path = argv[1];
    std::string dest_path = argv[2];
    if (is_directory(dest_path)) {
        std::filesystem::path src{src_path};
        std::filesystem::path dest{dest_path};
        dest /= src.filename(); 
        dest_path = dest.string();
    }
    if (is_same_file(src_path, dest_path)) {
        std::println(stderr, "copy: ORIGEN y DESTINO son el mismo archivo");
        return EXIT_FAILURE;
    }
    auto result = copy_file(src_path, dest_path);
    if (!result) {
        std::println(stderr, "copy: {}", result.error().what());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}