package main

import "core:container/lru"
import "core:hash"
import "core:log"
import "core:mem"

_print_debugs := ODIN_DEBUG

_debug_printfs : lru.Cache(u32, bool)

@(init)
init_debug_printfs :: proc() {
	if !_print_debugs {
		return
	}
	lru.init(&_debug_printfs, 1024)
}

// De-dup by arguments, so code doesn't need to call fmt.tprintf before throwing
// away the result (wasted time).
debug_printf_once :: proc(fmt: string, args: ..any, location := #caller_location) {
	if !_print_debugs {
		return
	}
	my_hash_fun :: hash.murmur32
	hash_key := my_hash_fun(mem.any_to_bytes(fmt))
	hash_key = my_hash_fun(mem.any_to_bytes(location), hash_key)
	for arg in args {
		arg_bytes := mem.any_to_bytes(arg)
		hash_key = my_hash_fun(arg_bytes, hash_key)
	}
	if !lru.exists(&_debug_printfs, hash_key) {
		lru.set(&_debug_printfs, hash_key, true)
		log.debugf(fmt_str=fmt, args=args, location=location)
	}
}
