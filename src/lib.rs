#![allow(non_upper_case_globals)]
#![allow(non_camel_case_types)]
#![allow(non_snake_case)]

pub mod ffi;

#[cfg(test)]
mod test {
    use std::ffi::CStr;

    use super::ffi::*;

    #[test]
    fn test_print_version() {
        let ver = unsafe { CStr::from_ptr(msc_version()) };
        println!("{}", ver.to_string_lossy());
        unsafe { msc_free_string(ver.as_ptr()) };
    }
}
