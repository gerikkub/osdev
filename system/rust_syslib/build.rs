use std::env;
use std::path::PathBuf;

fn main() {
    let libdir_path = PathBuf::from("../lib")
        .canonicalize()
        .expect("Cannot canonicalize path");

    let headers_path = libdir_path.join("system_console.h");
    let headers_path_str = headers_path.to_str().expect("Path is not a valid string");

    let bindings = bindgen::Builder::default()
        .header(headers_path_str)
        .clang_arg("--target=aarch64-unknown-none")
        .clang_arg(format!("-isysroot={}", env::var("CROSS_DIR").unwrap()))
        .use_core()
        .parse_callbacks(Box::new(bindgen::CargoCallbacks::new()))
        .generate()
        .expect("Unable to generate bindings");

    let out_path = PathBuf::from(env::var("OUT_DIR").unwrap());
    bindings.write_to_file(out_path.join("bindings.rs"))
            .expect("Couldn't write bindings");
}
