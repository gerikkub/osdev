#![no_std]
#![no_main]

use core::panic::PanicInfo;
use core::ffi;
use rust_syslib::console_write;

#[allow(dead_code)]
#[no_mangle]
fn main() {
    unsafe {
        let str = ffi::CStr::from_ptr(c"Hello Rust!".as_ptr());
        console_write(str.as_ptr());
    }
}



//#[panic_handler]
//fn panic_noop(_info: &PanicInfo) -> ! {
    //loop {}
//}
