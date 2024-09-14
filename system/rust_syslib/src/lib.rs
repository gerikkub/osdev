#![no_std]

#![allow(non_upper_case_globals)]
#![allow(non_camel_case_types)]
#![allow(non_snake_case)]

include!(concat!(env!("OUT_DIR"), "/bindings.rs"));

use core::panic::PanicInfo;

#[panic_handler]
fn panic_noop(_info: &PanicInfo) -> ! {
    loop {}
}
