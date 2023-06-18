fn main() {
    println!("cargo:rustc-link-lib=rusty_msc");
    println!("cargo:rustc-link-search=./deps");
    println!("cargo:rustc-flags=-l dylib=stdc++ -l dylib=X11");
}
