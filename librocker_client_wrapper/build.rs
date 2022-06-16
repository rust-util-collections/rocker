fn main() {
    println!("cargo:rustc-flags=-L {}", include!("ld_path"));
}
