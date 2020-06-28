//  gravity_core.c
//  gravity
//
//  Created by Marco Bambini on 10/01/15.
//  Copyright (c) 2015 CreoLabs. All rights reserved.
//
#include <gravity_.h>
#pragma hdrstop

// Gravity base classes (the isa pointer in each object).
// Null and Undefined points to same class (Null) and they
// differs from the n field inside GravityValue.
// n == 0 means NULL while n == 1 means UNDEFINED so I can
// reuse the same methods for both.
//
// Intrinsic datatypes are:
// - Int
// - Float
// - Boolean
// - String
// For these classes 4 conveniente conversion methods are provided.

// How internal conversion works
//
// Conversion is driven by the v1 class, so v2 is usually converter to v1 class
// and if the result is not as expected (very likely in complex expression) then the user
// is invited to explicitly cast values to the desired types.
// If a propert conversion function is not found then a runtime error is raised.

// Special note about Integer class
//
// Integer not always drives conversion based on v1 class
// that's because we are trying to fix the common case where
// an integer is added to a float.
// Without the smart check an operation like:
// 1 + 2.3 will result in 3
// So the first check is about v2 class (v1 is known) and if v2 class is float
// then v1 is converted to float and the propert operator_float_* function is called.

// Special note about Integer class
//
// Bitshift Operators does not make any sense for floating point values
// as pointed out here: http://www.cs.umd.edu/class/sum2003/cmsc311/Notes/BitOp/bitshift.html
// a trick could be to use a pointer to an int to actually manipulate
// floating point value. Since this is more a trick then a real solution
// I decided to cast v2 to integer without any extra check.
// Only operator_float_bit* functions are affected by this trick.

// Special note about Null class
//
// Every value in gravity is initialized to Null
// and can participate in math operations.
// This class should be defined in a way to do be
// less dangerous as possible and a Null value should
// be interpreted as a zero number (where possible).

static bool core_inited = false; // initialize global classes just once
static uint32 refcount = 0;      // protect deallocation of global classes

GravityGlobals GravityEnv;

#define CHECK_VALID(_check,_v,_msg)        if((_check) && (!_v)) RETURN_ERROR(_msg)
#define INTERNAL_CONVERT_FLOAT(_v,_check)  _v = convert_value2float(vm,_v); CHECK_VALID(_check,_v, "Unable to convert object to Float")
#define INTERNAL_CONVERT_BOOL(_v,_check)   _v = convert_value2bool(vm,_v); CHECK_VALID(_check,_v, "Unable to convert object to Bool")
#define INTERNAL_CONVERT_INT(_v,_check)    _v = convert_value2int(vm,_v); CHECK_VALID(_check,_v, "Unable to convert object to Int")
#define INTERNAL_CONVERT_STRING(_v,_check) _v = convert_value2string(vm,_v); CHECK_VALID(_check,_v, "Unable to convert object to String")

// boxed
//gravity_class_t * gravity_class_int_;
//gravity_class_t * gravity_class_float_;
//gravity_class_t * gravity_class_bool_;
//gravity_class_t * gravity_class_null_;
// objects
//gravity_class_t * gravity_class_string_;
//gravity_class_t * gravity_class_object_;
//gravity_class_t * gravity_class_function_;
//gravity_class_t * gravity_class_closure_;
//gravity_class_t * gravity_class_fiber_;
//gravity_class_t * gravity_class_class_;
//gravity_class_t * gravity_class_instance_;
//gravity_class_t * gravity_class_module_;
//gravity_class_t * gravity_class_list_;
//gravity_class_t * gravity_class_map_;
//gravity_class_t * gravity_class_range_;
//gravity_class_t * gravity_class_upvalue_;
//gravity_class_t * gravity_class_system_;

enum number_format_type {
	number_format_any,
	number_format_int,
	number_format_float
};

enum introspection_info_type {
	introspection_info_all,
	introspection_info_methods,
	introspection_info_variables
};

// MARK: - Utils -
static void map_keys_array(gravity_hash_t * hashtable, GravityValue key, GravityValue value, void * data) 
{
    //#pragma unused (hashtable, value)
	gravity_list_t * list = static_cast<gravity_list_t *>(data);
	list->array.insert(key);
}

// MARK: - Conversions -

static GravityValue convert_string2number(gravity_string_t * string, number_format_type number_format) 
{
	// empty string case
	if(string->len == 0) 
		return (number_format == number_format_float) ? GravityValue::from_float(0.0) : GravityValue::from_int(0);
	const char * s = string->s;
	uint32 len = string->len;
	int32 sign = 1;
	// check sign first
	if(oneof2(s[0], '-', '+')) {
		if(s[0] == '-') 
			sign = -1;
		++s; 
		--len;
	}
	// check special HEX, OCT, BIN cases
	if((s[0] == '0') && (len > 2)) {
		int c = toupper(s[1]);
		bool isHexBinOct = oneof3(c, 'B', 'O', 'X');
		if(isHexBinOct) {
			int64_t n = 0;
			if(c == 'B') n = number_from_bin(&s[2], len-2);
			else if(c == 'O') n = number_from_oct(&s[2], len-2);
			else if(c == 'X') n = number_from_hex(s, len);
			if(sign == -1) n = -n;
			return (number_format == number_format_float) ? GravityValue::from_float((gravity_float_t)n) : GravityValue::from_int((gravity_int_t)n);
		}
	}
	// if dot character is contained into the string than force the float_preferred flag
	if(number_format == number_format_any && (strchr(string->s, '.') != NULL)) 
		number_format = number_format_float;
	// default case
	return (number_format == number_format_float) ? GravityValue::from_float(strtod(string->s, NULL)) : GravityValue::from_int(strtoll(string->s, NULL, 0));
}

static bool convert_object_int(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    #pragma unused(vm, nargs)
	GravityValue v = convert_value2int(vm, args[0]);
	if(!v) 
		RETURN_ERROR("Unable to convert object to Int.");
	RETURN_VALUE(v, rindex);
}

static bool convert_object_float(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    #pragma unused(vm, nargs)
	GravityValue v = convert_value2float(vm, args[0]);
	if(!v) 
		RETURN_ERROR("Unable to convert object to Float.");
	RETURN_VALUE(v, rindex);
}

static bool convert_object_bool(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    #pragma unused(vm, nargs)
	GravityValue v = convert_value2bool(vm, args[0]);
	if(!v)
		RETURN_ERROR("Unable to convert object to Bool.");
	RETURN_VALUE(v, rindex);
}

static bool convert_object_string(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    #pragma unused(vm, nargs)
	GravityValue v = convert_value2string(vm, args[0]);
	if(!v) 
		RETURN_ERROR("Unable to convert object to String.");
	RETURN_VALUE(v, rindex);
}

static inline GravityValue convert_map2string(gravity_vm * vm, gravity_map_t * map) 
{
	// allocate initial memory to a 512 buffer
	uint32 len = 512;
	char * buffer = static_cast<char *>(mem_alloc(NULL, len+1));
	buffer[0] = '[';
	uint32 pos = 1;
	// get keys list
	uint32 count = gravity_hash_count(map->hash);
	gravity_list_t * list = gravity_list_new(vm, count);
	gravity_hash_iterate(map->hash, map_keys_array, (void*)list);
	count = list->array.getCount();
	for(uint32 i = 0; i < count; ++i) {
		GravityValue key = list->array.at(i);
		GravityValue * valueptr = gravity_hash_lookup(map->hash, key);
		GravityValue value = (valueptr) ? *valueptr : GravityValue::from_null();
		// key conversion
		if(!key.IsString()) {
			key = convert_value2string(vm, key);
		}
		gravity_string_t * key_string = (key.IsString()) ? static_cast<gravity_string_t *>(key) : 0;
		// value conversion
		if(value.IsMap() && (VALUE_AS_MAP(value) == map)) {
			// to avoid infinite loop
			value = GravityValue::from_null();
		}
		if(!value.IsString()) {
			value = convert_value2string(vm, value);
		}
		gravity_string_t * value_string = value.IsString() ? static_cast<gravity_string_t *>(value) : 0;
		// KEY
		char * s1 = (key_string) ? key_string->s : "N/A";
		uint32 len1 = (key_string) ? key_string->len : 3;

		// VALUE
		char * s2 = (value_string) ? value_string->s : "N/A";
		uint32 len2 = (value_string) ? value_string->len : 3;

		// check if buffer needs to be reallocated
		if(len1 + len2 + pos + 4 > len) {
			len = (len1 + len2 + pos + 4) + len;
			buffer = static_cast<char *>(mem_realloc(NULL, buffer, len));
		}
		// copy key string to new buffer
		memcpy(buffer+pos, s1, len1);
		pos += len1;
		// copy ':' key/value separator
		memcpy(buffer+pos, ":", 1);
		pos += 1;
		// copy value string to new buffer
		memcpy(buffer+pos, s2, len2);
		pos += len2;
		// add entries separator
		if(i+1 < count) {
			memcpy(buffer+pos, ",", 1);
			pos += 1;
		}
	}

	// Write latest ] character
	memcpy(buffer+pos, "]", 1);
	buffer[++pos] = 0;

	GravityValue result = VALUE_FROM_STRING(vm, buffer, pos);
	mem_free(buffer);
	return result;
}

static /*inline*/ GravityValue convert_list2string(gravity_vm * vm, gravity_list_t * list) 
{
	// allocate initial memory to a 512 buffer
	uint32 len = 512;
	char * buffer = static_cast<char *>(mem_alloc(NULL, len+1));
	buffer[0] = '[';
	uint32 pos = 1;
	// loop to perform string concat
	uint32 count = list->array.getCount();
	for(uint32 i = 0; i<count; ++i) {
		GravityValue value = list->array.at(i);
		gravity_string_t * string;
		if(value.IsList() && static_cast<gravity_list_t *>(value) == list) {
			string = NULL;
		}
		else {
			GravityValue value2 = convert_value2string(vm, value);
			string = (!!value2) ? static_cast<gravity_string_t *>(value2) : 0;
		}
		char * s1 = (string) ? string->s : "N/A";
		uint32 len1 = (string) ? string->len : 3;
		// check if buffer needs to be reallocated
		if(len1+pos+2 > len) {
			len = (len1+pos+2) + len;
			buffer = static_cast<char *>(mem_realloc(NULL, buffer, len));
		}
		// copy string to new buffer
		memcpy(buffer+pos, s1, len1);
		pos += len1;
		// add separator
		if(i+1 < count) {
			memcpy(buffer+pos, ",", 1);
			pos += 1;
		}
	}
	// Write latest ] character
	memcpy(buffer+pos, "]", 1);
	buffer[++pos] = 0;
	GravityValue result = VALUE_FROM_STRING(vm, buffer, pos);
	mem_free(buffer);
	return result;
}

/*inline*/GravityValue convert_value2int(gravity_vm * vm, GravityValue v) 
{
	if(v.IsInt()) 
		return v;
	// handle conversion for basic classes
	if(v.IsFloat()) return GravityValue::from_int((gravity_int_t)v.f);
	if(v.IsBool()) return GravityValue::from_int(v.n);
	if(VALUE_ISA_NULL(v)) return GravityValue::from_int(0);
	if(VALUE_ISA_UNDEFINED(v)) return GravityValue::from_int(0);
	if(v.IsString()) return convert_string2number(static_cast<gravity_string_t *>(v), number_format_int);
	// check if class implements the Int method
	gravity_closure_t * closure = gravity_vm_fastlookup(vm, v.GetClass(), GRAVITY_INT_INDEX);
	// sanity check (and break recursion)
	if((!closure) || ((closure->f->tag == EXEC_TYPE_INTERNAL) && (closure->f->internal == convert_object_int)) ||
	    gravity_vm_getclosure(vm) == closure) return VALUE_FROM_ERROR(NULL);
	// execute closure and return its value
	if(gravity_vm_runclosure(vm, closure, v, NULL, 0)) 
		return gravity_vm_result(vm);
	return VALUE_FROM_ERROR(NULL);
}

/*inline*/GravityValue convert_value2float(gravity_vm * vm, GravityValue v) 
{
	if(v.IsFloat()) 
		return v;
	// handle conversion for basic classes
	if(v.IsInt()) 
		return GravityValue::from_float((gravity_float_t)v.n);
	if(v.IsBool()) 
		return GravityValue::from_float((gravity_float_t)v.n);
	if(VALUE_ISA_NULL(v)) 
		return GravityValue::from_float(0);
	if(VALUE_ISA_UNDEFINED(v)) 
		return GravityValue::from_float(0);
	if(v.IsString()) 
		return convert_string2number(static_cast<gravity_string_t *>(v), number_format_float);
	// check if class implements the Float method
	gravity_closure_t * closure = gravity_vm_fastlookup(vm, v.GetClass(), GRAVITY_FLOAT_INDEX);
	// sanity check (and break recursion)
	if((!closure) || ((closure->f->tag == EXEC_TYPE_INTERNAL) && (closure->f->internal == convert_object_float)) || gravity_vm_getclosure(vm) == closure) 
		return VALUE_FROM_ERROR(NULL);
	// execute closure and return its value
	if(gravity_vm_runclosure(vm, closure, v, NULL, 0)) 
		return gravity_vm_result(vm);
	return VALUE_FROM_ERROR(NULL);
}

/*inline*/GravityValue convert_value2bool(gravity_vm * vm, GravityValue v) 
{
	if(v.IsBool()) 
		return v;
	// handle conversion for basic classes
	if(v.IsInt()) return VALUE_FROM_BOOL(v.n != 0);
	if(v.IsFloat()) return VALUE_FROM_BOOL(v.f != 0.0);
	if(VALUE_ISA_NULL(v)) return VALUE_FROM_FALSE;
	if(VALUE_ISA_UNDEFINED(v)) return VALUE_FROM_FALSE;
	if(v.IsString()) {
		gravity_string_t * string = static_cast<gravity_string_t *>(v);
		if(string->len == 0) 
			return VALUE_FROM_FALSE;
		return VALUE_FROM_BOOL((strcmp(string->s, "false") != 0));
	}
	// check if class implements the Bool method
	gravity_closure_t * closure = gravity_vm_fastlookup(vm, v.GetClass(), GRAVITY_BOOL_INDEX);
	// sanity check (and break recursion)
	if((!closure) || ((closure->f->tag == EXEC_TYPE_INTERNAL) && (closure->f->internal == convert_object_bool)) ||
	    gravity_vm_getclosure(vm) == closure) return VALUE_FROM_BOOL(1);
	// execute closure and return its value
	if(gravity_vm_runclosure(vm, closure, v, NULL, 0)) return gravity_vm_result(vm);
	return VALUE_FROM_ERROR(NULL);
}

/*inline*/GravityValue convert_value2string(gravity_vm * vm, GravityValue v) 
{
	if(v.IsString()) 
		return v;
	// handle conversion for basic classes
	else if(v.IsInt()) {
		char buffer[512];
	#if GRAVITY_ENABLE_INT64
		snprintf(buffer, sizeof(buffer), "%" PRId64, v.n);
	#else
		snprintf(buffer, sizeof(buffer), "%d", v.n);
	#endif
		return VALUE_FROM_CSTRING(vm, buffer);
	}
	else if(v.IsBool()) 
		return VALUE_FROM_CSTRING(vm, (v.n) ? "true" : "false");
	else if(VALUE_ISA_NULL(v)) 
		return VALUE_FROM_CSTRING(vm, "null");
	else if(VALUE_ISA_UNDEFINED(v)) 
		return VALUE_FROM_CSTRING(vm, "undefined");
	else if(v.IsFloat()) {
		char buffer[512];
		snprintf(buffer, sizeof(buffer), "%g", v.f);
		return VALUE_FROM_CSTRING(vm, buffer);
	}
	else if(v.IsClass()) {
		const char * identifier = static_cast<gravity_class_t *>(v)->identifier;
		SETIFZ(identifier, "anonymous class");
		return VALUE_FROM_CSTRING(vm, identifier);
	}
	else if(v.IsFunction()) {
		const char * identifier = (VALUE_AS_FUNCTION(v)->identifier);
		SETIFZ(identifier, "anonymous func");
		return VALUE_FROM_CSTRING(vm, identifier);
	}
	else if(v.IsClosure()) {
		const char * identifier = (VALUE_AS_CLOSURE(v)->f->identifier);
		SETIFZ(identifier, "anonymous closure");
		return VALUE_FROM_CSTRING(vm, identifier);
	}
	else if(v.IsList()) {
		gravity_list_t * list = static_cast<gravity_list_t *>(v);
		return convert_list2string(vm, list);
	}
	else if(v.IsMap()) {
		gravity_map_t * map = VALUE_AS_MAP(v);
		return convert_map2string(vm, map);
	}
	else if(v.IsRange()) {
		gravity_range_t * r = VALUE_AS_RANGE(v);
		char buffer[512];
		snprintf(buffer, sizeof(buffer), "%" PRId64 "...%" PRId64, r->from, r->to);
		return VALUE_FROM_CSTRING(vm, buffer);
	}
	else if(v.IsFiber()) {
		char buffer[512];
		snprintf(buffer, sizeof(buffer), "Fiber %p", VALUE_AS_OBJECT(v));
		return VALUE_FROM_CSTRING(vm, buffer);
	}
	else {
		// check if class implements the String method (avoiding infinite loop by checking for convert_object_string)
		gravity_closure_t * closure = gravity_vm_fastlookup(vm, v.GetClass(), GRAVITY_STRING_INDEX);
		// sanity check (and break recursion)
		if((!closure) || ((closure->f->tag == EXEC_TYPE_INTERNAL) && (closure->f->internal == convert_object_string)) ||
			gravity_vm_getclosure(vm) == closure) {
			if(v.IsInstance()) {
				gravity_instance_t * instance = VALUE_AS_INSTANCE(v);
				if(vm && instance->xdata) {
					gravity_delegate_t * delegate = gravity_vm_delegate(vm);
					if(delegate->bridge_string) {
						uint32 len = 0;
						const char * s = delegate->bridge_string(vm, instance->xdata, &len);
						if(s) 
							return VALUE_FROM_STRING(vm, s, len);
					}
				}
				else {
					char buffer[512];
					const char * identifier = (instance->objclass->identifier);
					SETIFZ(identifier, "anonymous class");
					snprintf(buffer, sizeof(buffer), "instance of %s (%p)", identifier, instance);
					return VALUE_FROM_CSTRING(vm, buffer);
				}
			}
			return VALUE_FROM_ERROR(NULL);
		}
		// execute closure and return its value
		if(gravity_vm_runclosure(vm, closure, v, NULL, 0)) {
			GravityValue result = gravity_vm_result(vm);
			// sanity check closure return value because sometimes nil is returned by an objc instance (for example
			// NSData String)
			if(!result.IsString()) 
				return VALUE_FROM_CSTRING(vm, "null");
			else
				return result;
		}
		return VALUE_FROM_ERROR(NULL);
	}
}

// MARK: - Object Introspection -

static bool object_class(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused(vm, nargs)
	gravity_class_t * c = args[0].GetClass();
	RETURN_VALUE(VALUE_FROM_OBJECT(c), rindex);
}

static bool object_meta(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused(vm, nargs)
	gravity_class_t * c = args[0].GetClass();
	RETURN_VALUE(VALUE_FROM_OBJECT(gravity_class_get_meta(c)), rindex);
}

static bool object_respond(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused(vm, nargs)
	gravity_class_t * c = args[0].GetClass();
	GravityValue key = args[1];
	bool result = false;
	if(key.IsString()) 
		result = (gravity_class_lookup(c, key) != NULL);
	RETURN_VALUE(VALUE_FROM_BOOL(result), rindex);
}

static void collect_introspection(gravity_hash_t * hashtable, GravityValue key, GravityValue value, void * data1, void * data2, void * data3) 
{
    //#pragma unused (hashtable, data3)
	gravity_list_t * list = static_cast<gravity_list_t *>(data1);
	introspection_info_type mask = *((introspection_info_type*)data2);
	// EXEC_TYPE_NATIVE     = 0
	// EXEC_TYPE_INTERNAL   = 1
	// EXEC_TYPE_BRIDGED    = 2
	// EXEC_TYPE_SPECIAL    = 3
	gravity_closure_t * closure = VALUE_AS_CLOSURE(value);
	gravity_function_t * func = closure->f;
	bool is_var = (func->tag == EXEC_TYPE_SPECIAL);
	if((mask == introspection_info_all) || ((mask == introspection_info_variables) && (is_var)) || ((mask == introspection_info_methods) && (!is_var))) {
		list->array.insert(key);
	}
}

static void collect_introspection_extended(gravity_hash_t * hashtable, GravityValue key, GravityValue value, void * data1, void * data2, void * data3) 
{
    //#pragma unused (hashtable)
	if(!value.IsClosure()) 
		return;
	gravity_map_t * map = (gravity_map_t*)data1;
	introspection_info_type mask = *((introspection_info_type*)data2);
	gravity_vm * vm = (gravity_vm *)data3;
	gravity_closure_t * closure = VALUE_AS_CLOSURE(value);
	gravity_function_t * func = closure->f;
	bool is_var = (func->tag == EXEC_TYPE_SPECIAL);
	// create info map
	gravity_map_t * info = gravity_map_new(vm, 16);
	// name
	// description?
	// isvar
	// index
	// read-only
	// type?
	// parameters (array of maps)
	// name
	// type?
	// index
	// value?

	gravity_map_insert(vm, info, VALUE_FROM_CSTRING(vm, "name"), VALUE_FROM_CSTRING(vm, static_cast<gravity_string_t *>(key)->s));
	gravity_map_insert(vm, info, VALUE_FROM_CSTRING(vm, "isvar"), VALUE_FROM_BOOL(is_var));
	if(is_var) {
		if(func->index < GRAVITY_COMPUTED_INDEX) 
			gravity_map_insert(vm, info, VALUE_FROM_CSTRING(vm, "index"), GravityValue::from_int(func->index));
		gravity_map_insert(vm, info, VALUE_FROM_CSTRING(vm, "readonly"), VALUE_FROM_BOOL(func->special[0] != NULL && func->special[1] == NULL));
	}
	else {
		gravity_list_t * params = gravity_function_params_get(vm, func);
		if(params) 
			gravity_map_insert(vm, info, VALUE_FROM_CSTRING(vm, "params"), VALUE_FROM_OBJECT(reinterpret_cast<gravity_object_t *>(params)));
	}

	if((mask == introspection_info_all) || ((mask == introspection_info_variables) && (is_var)) || ((mask == introspection_info_methods) && (!is_var))) {
		gravity_map_insert(vm, map, key, VALUE_FROM_OBJECT(reinterpret_cast<gravity_object_t *>(info)));
	}
}

static bool FASTCALL object_real_introspection(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex, introspection_info_type mask) 
{
	bool extended = ((nargs >= 2) && (args[1].IsBool() && (VALUE_AS_BOOL(args[1]) == true)));
	bool scan_super = ((nargs >= 3) && (args[2].IsBool() && (VALUE_AS_BOOL(args[2]) == true)));
	gravity_hash_iterate3_fn callback = (extended) ? collect_introspection_extended : collect_introspection;
	gravity_object_t * data = (extended) ? (gravity_object_t *)gravity_map_new(vm, 256) : (gravity_object_t *)gravity_list_new(vm, 256);
	GravityValue value = args[0];
	gravity_class_t * c = value.IsClass() ? static_cast<gravity_class_t *>(value) : value.GetClass();
	while(c) {
		gravity_hash_t * htable = c->htable;
		gravity_hash_iterate3(htable, callback, (void*)data, (void*)&mask, (void*)vm);
		c = (!scan_super) ? NULL : c->superclass;
	}
	RETURN_VALUE(VALUE_FROM_OBJECT(data), rindex);
}

static bool object_methods(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
	{ return object_real_introspection(vm, args, nargs, rindex, introspection_info_methods); }
static bool object_properties(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
	{ return object_real_introspection(vm, args, nargs, rindex, introspection_info_variables); }
static bool object_introspection(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
	{ return object_real_introspection(vm, args, nargs, rindex, introspection_info_all); }

// MARK: - Object Class -

static bool object_internal_size(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    #pragma unused(vm, nargs)
	gravity_int_t size = gravity_value_size(vm, args[0]);
	if(size == 0) size = sizeof(GravityValue);
	RETURN_VALUE(GravityValue::from_int(size), rindex);
}

static bool object_is(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    #pragma unused(vm, nargs)
	gravity_class_t * c1 = args[0].GetClass();
	gravity_class_t * c2 = static_cast<gravity_class_t *>(args[1]);
	while(c1 != c2 && c1->superclass != NULL) {
		c1 = c1->superclass;
	}
	RETURN_VALUE(VALUE_FROM_BOOL(c1 == c2), rindex);
}

static bool object_eqq(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    #pragma unused(vm, nargs)
	GravityValue v1 = args[0];
	GravityValue v2 = args[1];
	// compare class first
	if(v1.GetClass() != v2.GetClass())
		RETURN_VALUE(VALUE_FROM_FALSE, rindex);
	// then compare value
	RETURN_VALUE(VALUE_FROM_BOOL(gravity_value_vm_equals(vm, v1, v2)), rindex);
}

static bool object_neqq(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
	object_eqq(vm, args, nargs, rindex);
	GravityValue value = args[rindex];
	if(value.IsBool()) {
		RETURN_VALUE(VALUE_FROM_BOOL(!VALUE_AS_BOOL(value)), rindex);
	}
	return true;
}

static bool object_cmp(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused(vm, nargs)
	if(gravity_value_vm_equals(vm, args[0], args[1])) 
		RETURN_VALUE(GravityValue::from_int(0), rindex);
	RETURN_VALUE(GravityValue::from_int(1), rindex);
}

static bool object_not(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused(vm, nargs)
	// !obj
	// if obj is NULL then result is true
	// everything else must be false
	RETURN_VALUE(VALUE_FROM_BOOL(VALUE_ISA_NULLCLASS(args[0])), rindex);
}

static bool object_load(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused(vm, nargs)
	// if there is a possibility that gravity_vm_runclosure is called then it is MANDATORY to save arguments before
	// the call
	GravityValue target = args[0];
	GravityValue key = args[1];
	// check if meta class needs to be initialized (it means if it contains valued static ivars)
	// meta classes must be inited somewhere, this problem does not exist with instances since object creation
	// itself trigger a class init
	if(target.IsClass()) {
		gravity_class_t * c = static_cast<gravity_class_t *>(target);
		gravity_class_t * meta = gravity_class_get_meta(c);
		if(!meta->is_inited) {
			meta->is_inited = true;
			gravity_closure_t * closure = gravity_class_lookup_constructor(meta, 0);
			if(closure) {
				if(!gravity_vm_runclosure(vm, closure, VALUE_FROM_OBJECT(meta), NULL, 0)) return false;
			}
		}
	}
	// retrieve class and process key
	gravity_class_t * c = target.GetClass();
	gravity_instance_t * instance = target.IsInstance() ? VALUE_AS_INSTANCE(target) : NULL;
	// key is an int its an optimization for faster loading of ivar
	if(key.IsInt()) {
		// sanity check
		uint32 nivar = c->nivars;
		uint32 nindex = (uint32)key.n;
		if(nindex >= nivar) 
			RETURN_ERROR("Out of bounds ivar index in load operation (1).");
		if(instance) 
			RETURN_VALUE(instance->ivars[nindex], rindex); // instance case
		RETURN_VALUE(c->ivars[nindex], rindex);                 // class case
	}
	// key must be a string in this version
	if(!key.IsString()) {
		RETURN_ERROR("Unable to lookup non string value into class %s", c->identifier);
	}
	// lookup key in class c
	gravity_object_t * obj = (gravity_object_t *)gravity_class_lookup(c, key);
	if(!obj) {
		// not explicitly declared so check for dynamic property in bridge case
		gravity_delegate_t * delegate = gravity_vm_delegate(vm);
		if(instance && instance->xdata && delegate->bridge_getundef) {
			if(delegate->bridge_getundef(vm, instance->xdata, target, key.GetZString(), rindex)) 
				return true;
		}
	}
	if(!obj) 
		goto execute_notfound;
	if(obj->IsClosure()) {
		gravity_closure_t * closure = (gravity_closure_t *)obj;
		if(!closure || !closure->f) 
			goto execute_notfound;
		// execute optimized default getter
		if(FUNCTION_ISA_SPECIAL(closure->f)) {
			if(FUNCTION_ISA_DEFAULT_GETTER(closure->f)) {
				// sanity check
				uint32 nivar = c->nivars;
				uint32 nindex = closure->f->index;
				if(nindex >= nivar) 
					RETURN_ERROR("Out of bounds ivar index in load operation (2).");
				if(instance) 
					RETURN_VALUE(instance->ivars[closure->f->index], rindex);
				RETURN_VALUE(c->ivars[closure->f->index], rindex);
			}
			if(FUNCTION_ISA_GETTER(closure->f)) {
				// returns a function to be executed using the return false trick
				RETURN_CLOSURE(VALUE_FROM_OBJECT(reinterpret_cast<gravity_object_t *>((gravity_closure_t *)closure->f->special[EXEC_TYPE_SPECIAL_GETTER])), rindex);
			}
			goto execute_notfound;
		}
	}

	RETURN_VALUE(VALUE_FROM_OBJECT(obj), rindex);

execute_notfound: {
		// in case of not found error return the notfound function to be executed (MANDATORY)
		gravity_closure_t * closure = (gravity_closure_t *)gravity_class_lookup(c, gravity_vm_keyindex(vm, GRAVITY_NOTFOUND_INDEX));
		RETURN_CLOSURE(VALUE_FROM_OBJECT(reinterpret_cast<gravity_object_t *>(closure)), rindex);
	}
}

static bool object_store(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused(vm, nargs, rindex)
	// if there is a possibility that gravity_vm_runfunc is called then it is MANDATORY to save arguments before the
	// call
	GravityValue target = args[0];
	GravityValue key = args[1];
	GravityValue value = args[2];
	// check if meta class needs to be initialized (it means if it contains valued static ivars)
	// meta classes must be inited somewhere, this problem does not exist with classes since object creation itself
	// trigger a class init
	if(target.IsClass()) {
		gravity_class_t * c = static_cast<gravity_class_t *>(target);
		gravity_class_t * meta = gravity_class_get_meta(c);
		if(!meta->is_inited) {
			meta->is_inited = true;
			gravity_closure_t * closure = gravity_class_lookup_constructor(meta, 0);
			if(closure) {
				if(!gravity_vm_runclosure(vm, closure, VALUE_FROM_OBJECT(meta), NULL, 0)) return false;
			}
		}
	}

	// retrieve class and process key
	gravity_class_t * c = target.GetClass();
	gravity_instance_t * instance = target.IsInstance() ? VALUE_AS_INSTANCE(target) : NULL;

	// key is an int its an optimization for faster loading of ivar
	if(key.IsInt()) {
		// sanity check
		uint32 nivar = c->nivars;
		uint32 nindex = (uint32)key.n;
		if(nindex >= nivar) 
			RETURN_ERROR("Out of bounds ivar index in store operation (1).");
		// check for struct
		if(value.IsInstance() && (gravity_instance_isstruct(VALUE_AS_INSTANCE(value)))) {
			gravity_instance_t * instance_copy = gravity_instance_clone(vm, VALUE_AS_INSTANCE(value));
			value = VALUE_FROM_OBJECT(reinterpret_cast<gravity_object_t *>(instance_copy));
		}
		if(instance) 
			instance->ivars[nindex] = value;
		else 
			c->ivars[nindex] = value;
		RETURN_NOVALUE();
	}
	// key must be a string in this version
	if(!key.IsString()) {
		RETURN_ERROR("Unable to lookup non string value into class %s", c->identifier);
	}
	gravity_object_t * obj = gravity_class_lookup(c, key); // lookup key in class c
	if(!obj) {
		// not explicitly declared so check for dynamic property in bridge case
		gravity_delegate_t * delegate = gravity_vm_delegate(vm);
		if(instance && instance->xdata && delegate->bridge_setundef) {
			if(delegate->bridge_setundef(vm, instance->xdata, target, key.GetZString(), value)) RETURN_NOVALUE();
		}
	}
	if(!obj) 
		goto execute_notfound;
	gravity_closure_t * closure;
	if(obj->IsClosure()) {
		closure = (gravity_closure_t *)obj;
		if(!closure || !closure->f) 
			goto execute_notfound;
		// check for special functions case
		if(FUNCTION_ISA_SPECIAL(closure->f)) {
			// execute optimized default setter
			if(FUNCTION_ISA_DEFAULT_SETTER(closure->f)) {
				// sanity check
				uint32 nivar = c->nivars;
				uint32 nindex = closure->f->index;
				if(nindex >= nivar) 
					RETURN_ERROR("Out of bounds ivar index in store operation (2).");
				// check for struct
				if(value.IsInstance() && (gravity_instance_isstruct(VALUE_AS_INSTANCE(value)))) {
					gravity_instance_t * instance_copy = gravity_instance_clone(vm, VALUE_AS_INSTANCE(value));
					value = VALUE_FROM_OBJECT(reinterpret_cast<gravity_object_t *>(instance_copy));
				}
				if(instance) 
					instance->ivars[nindex] = value;
				else 
					c->ivars[nindex] = value;
				RETURN_NOVALUE();
			}
			if(FUNCTION_ISA_SETTER(closure->f)) {
				// returns a function to be executed using the return false trick
				RETURN_CLOSURE(VALUE_FROM_OBJECT(reinterpret_cast<gravity_object_t *>((gravity_closure_t *)closure->f->special[EXEC_TYPE_SPECIAL_SETTER])), rindex);
			}
			goto execute_notfound;
		}
	}
	RETURN_NOVALUE();
execute_notfound:
	// in case of not found error return the notfound function to be executed (MANDATORY)
	closure = (gravity_closure_t *)gravity_class_lookup(c, gravity_vm_keyindex(vm, GRAVITY_NOTFOUND_INDEX));
	RETURN_CLOSURE(VALUE_FROM_OBJECT(reinterpret_cast<gravity_object_t *>(closure)), rindex);
}

static bool object_notfound(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused(nargs,rindex)
	gravity_class_t * c = args[0].GetClass();
	GravityValue key = args[1]; // vm_getslot(vm, rindex);
	RETURN_ERROR("Unable to find %s into class %s", key.IsString() ? key.GetZString() : "N/A", c->identifier);
}

static bool object_bind(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused(rindex)
	// sanity check first
	if(nargs < 3) 
		RETURN_ERROR("Incorrect number of arguments.");
	if(!args[1].IsString()) 
		RETURN_ERROR("First argument must be a String.");
	if(!args[2].IsClosure()) 
		RETURN_ERROR("Second argument must be a Closure.");
	gravity_object_t * object = NULL;
	if(args[0].IsInstance() || args[0].IsClass()) {
		object = VALUE_AS_OBJECT(args[0]);
	}
	else {
		RETURN_ERROR("bind method can be applied only to instances or classes.");
	}
	gravity_string_t * key = static_cast<gravity_string_t *>(args[1]);
	gravity_class_t * c = args[0].GetClass();
	// in this version core classes are shared among all VM instances and
	// this could be an issue in case of bound methods so it would be probably
	// a good idea to play safe and forbid bind on core classes
	if(gravity_iscore_class(c)) {
		RETURN_ERROR("Unable to bind method to a Gravity core class.");
	}

	// check if instance has already an anonymous class added to its hierarchy
	if(!gravity_class_is_anon(c)) {
		// no super anonymous class found so create a new one, set its super as c, and add it to the hierarchy
		char * name = gravity_vm_anonymous(vm);

		// cg needs to be disabled here because it could run inside class allocation triggering a free for the
		// meta class
		gravity_gc_setenabled(vm, false);
		gravity_class_t * anon = gravity_class_new_pair(vm, name, c, 0, 0);
		gravity_gc_setenabled(vm, true);
		object->objclass = anon;
		c = anon;

		// store anonymous class (and its meta) into VM context
		gravity_vm_setvalue(vm, name, VALUE_FROM_OBJECT(anon));
	}

	// set closure context (only if class or instance)
	// VALUE_AS_CLOSURE(args[2])->context = object;

	// add closure to anonymous class
	gravity_class_bind(c, key->s, args[2]);
	RETURN_NOVALUE();
}

static bool object_unbind(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    #pragma unused(rindex)
	// sanity check first
	if(nargs < 2) 
		RETURN_ERROR("Incorrect number of arguments.");
	if(!args[1].IsString()) 
		RETURN_ERROR("Argument must be a String.");
	// remove key/value from hash table
	gravity_class_t * c = args[0].GetClass();
	gravity_object_t * obj = (gravity_object_t *)gravity_class_lookup(c, args[1]);
	// clear closure context
	if(obj && obj->IsClosure()) 
		reinterpret_cast<gravity_closure_t *>(obj)->context = NULL;
	// remove key from class hash table
	gravity_hash_remove(c->htable, args[1]);
	RETURN_NOVALUE();
}

static bool object_exec(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused(vm, args, nargs)
	RETURN_ERROR("Forbidden Object execution.");
}

static bool object_clone(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused(nargs)
	if(args[0].IsInstance()) {
		gravity_instance_t * instance = (gravity_instance_t*)(args[0].p);
		gravity_instance_t * clone = gravity_instance_clone(vm, instance);
		RETURN_VALUE(VALUE_FROM_OBJECT(reinterpret_cast<gravity_object_t *>(clone)), rindex);
	}
	// more cases to add in the future
	RETURN_ERROR("Unable to clone non instance object.");
}

//static bool object_methods (gravity_vm *vm, GravityValue *args, uint16 nargs, uint32 rindex) {
//    #pragma unused(vm, nargs)
//    gravity_class_t *c = args[0].GetClass();
//
//
//}

// MARK: - List Class -

static bool list_count(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused(vm, nargs)
	gravity_list_t * list = static_cast<gravity_list_t *>(args[0]);
	RETURN_VALUE(GravityValue::from_int(list->array.getCount()), rindex);
}

static bool list_contains(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused(vm, nargs)
	gravity_list_t * list = static_cast<gravity_list_t *>(args[0]);
	GravityValue element = args[1];
	GravityValue result = VALUE_FROM_FALSE;
	uint32 count = list->array.getCount();
	uint   i = 0;
	while(i < count) {
		if(gravity_value_vm_equals(vm, list->array.at(i), element)) {
			result = VALUE_FROM_TRUE;
			break;
		}
		++i;
	}
	RETURN_VALUE(result, rindex);
}

static bool list_indexOf(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused(vm, nargs)
	gravity_list_t * list = static_cast<gravity_list_t *>(args[0]);
	GravityValue element = args[1];
	GravityValue result = GravityValue::from_int(-1);
	uint32 count = list->array.getCount();
	uint   i = 0;
	while(i < count) {
		if(gravity_value_vm_equals(vm, list->array.at(i), element)) {
			result = GravityValue::from_int(i);
			break;
		}
		++i;
	}
	RETURN_VALUE(result, rindex);
}

static bool list_loadat(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused(vm, nargs)
	gravity_list_t    * list = static_cast<gravity_list_t *>(args[0]);
	GravityValue value = args[1];
	if(!value.IsInt()) 
		RETURN_ERROR("An integer index is required to access a list item.");
	int32 index = (int32)value.GetInt();
	uint32 count = list->array.getCount();
	if(index < 0) 
		index = count + index;
	if((index < 0) || ((uint32)index >= count)) 
		RETURN_ERROR("Out of bounds error: index %d beyond bounds 0...%d", index, count-1);
	RETURN_VALUE(list->array.at(index), rindex);
}

static bool list_storeat(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused(vm, nargs, rindex)
	gravity_list_t    * list = static_cast<gravity_list_t *>(args[0]);
	GravityValue idxvalue = args[1];
	GravityValue value = args[2];
	if(!idxvalue.IsInt()) 
		RETURN_ERROR("An integer index is required to access a list item.");
	int32 index = (int32)idxvalue.GetInt();
	uint32 count = list->array.getCount();
	if(index < 0) 
		index = count + index;
	if(index < 0) 
		RETURN_ERROR("Out of bounds error: index %d beyond bounds 0...%d", index, count-1);
	if((uint32)index >= count) {
		// handle list resizing here
		list->array.resize(index-count+MIN_LIST_RESIZE);
		if(!list->array.p) 
			RETURN_ERROR("Not enough memory to resize List.");
		marray_nset(list->array, index+1);
		for(int32 i = count; i<=(index+MIN_LIST_RESIZE); ++i) {
			marray_set(list->array, i, GravityValue::from_null());
		}
		marray_set(list->array, index, value);
	}
	marray_set(list->array, index, value);
	RETURN_NOVALUE();
}

static bool list_push(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused(nargs)
	gravity_list_t * list = static_cast<gravity_list_t *>(args[0]);
	GravityValue value = args[1];
	list->array.insert(value);
	RETURN_VALUE(GravityValue::from_int(list->array.getCount()), rindex);
}

static bool list_pop(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused(nargs)
	gravity_list_t * list = static_cast<gravity_list_t *>(args[0]);
	size_t count = list->array.getCount();
	if(count < 1) 
		RETURN_ERROR("Unable to pop a value from an empty list.");
	GravityValue value = list->array.pop();
	RETURN_VALUE(value, rindex);
}

static bool list_remove(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused(nargs)
	gravity_list_t * list = static_cast<gravity_list_t *>(args[0]);
	if(!args[1].IsInt()) 
		RETURN_ERROR("Parameter must be of type Int.");
	gravity_int_t index = args[1].GetInt();
	size_t count = list->array.getCount();
	if((index < 0) || (index >= count)) 
		RETURN_ERROR("Out of bounds index.");
	// remove an item means move others down
	memmove(&list->array.p[index], &list->array.p[index+1], static_cast<size_t>(((count-1)-index) * sizeof(GravityValue)));
	list->array.n -= 1;
	RETURN_VALUE(args[0], rindex);
}

static bool list_iterator(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    #pragma unused(vm, nargs)
	gravity_list_t * list = static_cast<gravity_list_t *>(args[0]);
	// check for empty list first
	uint32 count = list->array.getCount();
	if(count == 0) 
		RETURN_VALUE(VALUE_FROM_FALSE, rindex);
	// check for start of iteration
	if(VALUE_ISA_NULL(args[1])) 
		RETURN_VALUE(GravityValue::from_int(0), rindex);
	// extract value
	GravityValue value = args[1];
	// check error condition
	if(!value.IsInt()) 
		RETURN_ERROR("Iterator expects a numeric value here.");
	// compute new value
	gravity_int_t n = value.n;
	if(n+1 < count) {
		++n;
	}
	else {
		RETURN_VALUE(VALUE_FROM_FALSE, rindex);
	}
	// return new iterator
	RETURN_VALUE(GravityValue::from_int(n), rindex);
}

static bool list_iterator_next(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    #pragma unused(vm, nargs)
	gravity_list_t * list = static_cast<gravity_list_t *>(args[0]);
	int32 index = (int32)args[1].GetInt();
	RETURN_VALUE(list->array.at(index), rindex);
}

static bool list_loop(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
	if(nargs < 2) 
		RETURN_ERROR("Incorrect number of arguments.");
	if(!args[1].IsClosure()) 
		RETURN_ERROR("Argument must be a Closure.");
	gravity_closure_t * closure = VALUE_AS_CLOSURE(args[1]); // closure to execute
	GravityValue value = args[0];                        // self parameter
	gravity_list_t * list = static_cast<gravity_list_t *>(value);
	uint   n = list->array.getCount(); // times to execute the loop
	uint   i = 0;
	nanotime_t t1 = nanotime();
	while(i < n) {
		if(!gravity_vm_runclosure(vm, closure, value, &list->array.at(i), 1)) 
			return false;
		++i;
	}
	nanotime_t t2 = nanotime();
	RETURN_VALUE(GravityValue::from_int(t2-t1), rindex);
}

static bool list_reverse(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
	if(nargs > 1) 
		RETURN_ERROR("Incorrect number of arguments.");
	GravityValue value = args[0];                      // self parameter
	gravity_list_t * list = static_cast<gravity_list_t *>(value);
	size_t count = list->array.getCount();
	size_t i = 0;
	while(i < count/2) {
		GravityValue tmp = list->array.at(count-i-1);
		marray_set(list->array, count-i-1,  list->array.at(i));
		marray_set(list->array, i,  tmp);
		i++;
	}
	RETURN_VALUE(VALUE_FROM_OBJECT(reinterpret_cast<gravity_object_t *>(list)), rindex);
}

static bool list_reversed(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
	if(nargs > 1) 
		RETURN_ERROR("Incorrect number of arguments.");
	// self parameter
	gravity_list_t * list = static_cast<gravity_list_t *>(args[0]);
	gravity_list_t * newlist = gravity_list_new(vm, (uint32)list->array.n);
	size_t count = list->array.getCount();
	size_t i = 0;
	while(i < count) {
		newlist->array.insert(list->array.at(count-i-1));
		++i;
	}
	RETURN_VALUE(VALUE_FROM_OBJECT(reinterpret_cast<gravity_object_t *>(newlist)), rindex);
}

typedef bool (list_comparison_callback) (gravity_vm * vm, GravityValue val1, GravityValue val2);

static bool list_default_number_compare(gravity_vm * vm, GravityValue val1, GravityValue val2) 
{
	gravity_float_t n1 = convert_value2float(vm, val1).f;
	gravity_float_t n2 = convert_value2float(vm, val2).f;
	return n1 > n2;
}

static bool list_default_string_compare(gravity_vm * vm, GravityValue val1, GravityValue val2) 
{
	gravity_string_t * s1 = static_cast<gravity_string_t *>(convert_value2string(vm, val1));
	gravity_string_t * s2 = static_cast<gravity_string_t *>(convert_value2string(vm, val2));
	return (strcmp(s1->s, s2->s) > 0);
}

static bool compare_values(gravity_vm * vm, GravityValue selfvalue, GravityValue val1, GravityValue val2, gravity_closure_t * predicate) 
{
	GravityValue params[2] = {val1, val2};
	if(!gravity_vm_runclosure(vm, predicate, selfvalue, params, 2)) 
		return false;
	GravityValue result = gravity_vm_result(vm);
	//the conversion will make sure that the comparison function only returns a
	//truthy value that can be interpreted as the result of a comparison
	//(i.e. only integer, bool, float, null, undefined, or string)
	GravityValue truthy_value = convert_value2bool(vm, result);
	return LOGIC(truthy_value.n);
}

static uint32 partition(gravity_vm * vm, GravityValue * array, int32 low, int32 high,
    GravityValue selfvalue, gravity_closure_t * predicate, list_comparison_callback * callback) 
{
	GravityValue pivot = array[high];
	int32 i = low - 1;
	for(int32 j = low; j <= high - 1; j++) {
		if((predicate && !compare_values(vm, selfvalue, array[j], pivot, predicate)) || (callback && !callback(vm, array[j], pivot))) {
			++i;
			GravityValue temp = array[i]; //swap a[i], a[j]
			array[i] = array[j];
			array[j] = temp;
		}
	}
	GravityValue temp = array[i + 1];
	array[i + 1] = array[high];
	array[high] = temp;
	return i + 1;
}

static void quicksort(gravity_vm * vm, GravityValue * array, int32 low, int32 high,
    GravityValue selfvalue, gravity_closure_t * predicate, list_comparison_callback * callback) 
{
	if(gravity_vm_isaborted(vm)) 
		return;
	if(low < high) {
		int32 pi = partition(vm, array, low, high, selfvalue, predicate, callback);
		quicksort(vm, array, low, pi - 1, selfvalue, predicate, callback);
		quicksort(vm, array, pi + 1, high, selfvalue, predicate, callback);
	}
}

static bool list_sort(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
	//the predicate is the comparison function passed to list.sort() if any
	gravity_closure_t * predicate = NULL;
	list_comparison_callback * callback = NULL;
	if(nargs >=2 && args[1].IsClosure()) 
		predicate = VALUE_AS_CLOSURE(args[1]);
	GravityValue selfvalue = args[0]; // self parameter
	gravity_list_t * list = static_cast<gravity_list_t *>(selfvalue);
	int32 count = list->array.getCount();
	if(count > 1) {
		if(predicate == NULL) {
			GravityValue first_value = list->array.at(0);
			if(first_value.IsInt() || first_value.IsFloat()) 
				callback = list_default_number_compare;
			else 
				callback = list_default_string_compare;
		}
		quicksort(vm, list->array.p, 0, count-1, selfvalue, predicate, callback);
	}
	RETURN_VALUE(VALUE_FROM_OBJECT((gravity_object_t *)list), rindex);
}

static bool list_sorted(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
	//the predicate is the comparison function passed to list.sort() if any
	gravity_closure_t * predicate = NULL;
	list_comparison_callback * callback = NULL;
	if(nargs >=2 && args[1].IsClosure()) 
		predicate = VALUE_AS_CLOSURE(args[1]);
	GravityValue selfvalue = args[0]; // self parameter
	gravity_list_t * list = static_cast<gravity_list_t *>(selfvalue);
	int32 count = (int32)list->array.getCount();
	// do not transfer newlist to GC because it could be freed during predicate closure execution
	// (because newlist is not yet in any stack)
	gravity_list_t * newlist = gravity_list_new(NULL, (uint32)count);

	//memcpy should be faster than pushing element by element
	memcpy(newlist->array.p, list->array.p, sizeof(GravityValue)*count);
	newlist->array.m = list->array.m;
	newlist->array.n = list->array.n;
	if(count > 1) {
		if(predicate == NULL) {
			GravityValue first_value = list->array.at(0);
			if(first_value.IsInt() || first_value.IsFloat()) 
				callback = list_default_number_compare;
			else 
				callback = list_default_string_compare;
		}
		quicksort(vm, newlist->array.p, 0, (int32)count-1, selfvalue, predicate, callback);
	}
	gravity_vm_transfer(vm, (gravity_object_t *)newlist);
	RETURN_VALUE(VALUE_FROM_OBJECT(reinterpret_cast<gravity_object_t *>(newlist)), rindex);
}

static bool list_map(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
	if(nargs != 2) 
		RETURN_ERROR("One argument is needed by the map function.");
	if(!args[1].IsClosure()) 
		RETURN_ERROR("Argument must be a Closure.");
	GravityValue selfvalue = args[0]; // self parameter
	gravity_closure_t * predicate = VALUE_AS_CLOSURE(args[1]);
	gravity_list_t * list = static_cast<gravity_list_t *>(selfvalue);
	size_t count = list->array.getCount();
	// do not transfer newlist to GC because it could be freed during predicate closure execution
	gravity_list_t * newlist = gravity_list_new(NULL, (uint32)count);
	newlist->array.m = list->array.m;
	newlist->array.n = list->array.n;
	for(uint32 i = 0; i < count; i++) {
		GravityValue * value = &list->array.at(i);
		if(!gravity_vm_runclosure(vm, predicate, selfvalue, value, 1)) 
			return false;
		GravityValue result = gravity_vm_result(vm);
		marray_set(newlist->array, i, result);
	}

	gravity_vm_transfer(vm, (gravity_object_t *)newlist);
	RETURN_VALUE(VALUE_FROM_OBJECT(reinterpret_cast<gravity_object_t *>(newlist)), rindex);
}

static bool list_filter(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
	if(nargs != 2) 
		RETURN_ERROR("One argument is needed by the filter function.");
	if(!args[1].IsClosure()) 
		RETURN_ERROR("Argument must be a Closure.");
	GravityValue selfvalue = args[0]; // self parameter
	gravity_closure_t * predicate = VALUE_AS_CLOSURE(args[1]);
	gravity_list_t * list = static_cast<gravity_list_t *>(selfvalue);
	size_t count = list->array.getCount();
	// do not transfer newlist to GC because it could be freed during predicate closure execution
	gravity_list_t * newlist = gravity_list_new(NULL, (uint32)count);
	for(uint32 i = 0; i < count; i++) {
		GravityValue * value = &list->array.at(i);
		if(!gravity_vm_runclosure(vm, predicate, selfvalue, value, 1)) return false;
		GravityValue result = gravity_vm_result(vm);
		GravityValue truthy_value = convert_value2bool(vm, result);
		if(truthy_value.n) {
			newlist->array.insert(*value);
		}
	}

	gravity_vm_transfer(vm, (gravity_object_t *)newlist);
	RETURN_VALUE(VALUE_FROM_OBJECT(reinterpret_cast<gravity_object_t *>(newlist)), rindex);
}

static bool list_reduce(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
	if(nargs != 3) 
		RETURN_ERROR("Two arguments are needed by the reduce function.");
	if(!args[2].IsClosure()) 
		RETURN_ERROR("Argument 2 must be a Closure.");
	GravityValue selfvalue = args[0]; // self parameter
	GravityValue start = args[1]; // start parameter
	gravity_closure_t * predicate = VALUE_AS_CLOSURE(args[2]);
	gravity_list_t * list = static_cast<gravity_list_t *>(selfvalue);
	size_t count = list->array.getCount();
	for(uint32 i = 0; i < count; i++) {
		GravityValue params[2] = {start, list->array.at(i)};
		if(!gravity_vm_runclosure(vm, predicate, selfvalue, params, 2)) return false;
		start = gravity_vm_result(vm);
	}
	RETURN_VALUE(start, rindex);
}

static bool list_join(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
	gravity_list_t * list = static_cast<gravity_list_t *>(args[0]);
	const char * sep = NULL;
	if((nargs > 1) && args[1].IsString()) sep = args[1].GetZString();
	// create a new empty buffer
	uint32 alloc = (uint32)(list->array.getCount() * 64);
	SETIFZ(alloc, 256);
	uint32 len = 0;
	uint32 seplen = (sep) ? static_cast<gravity_string_t *>(args[1])->len : 0;
	char * _buffer = static_cast<char *>(mem_alloc(vm, alloc));
	CHECK_MEM_ALLOC(_buffer);
	uint   n = list->array.getCount();
	uint   i = 0;
	// traverse list and append each item
	while(i < n) {
		GravityValue value = convert_value2string(vm, list->array.at(i));
		if(!value) {
			mem_free(_buffer);
			RETURN_VALUE(value, rindex);
		}

		// compute string to append
		const char * s2 = static_cast<gravity_string_t *>(value)->s;
		uint32 req = static_cast<gravity_string_t *>(value)->len;
		uint32 free_mem = alloc - len;
		// check if buffer needs to be reallocated
		if(free_mem < req + seplen + 1) {
			uint64_t to_alloc = alloc + (req + seplen) * 2 + 4096;
			_buffer = static_cast<char *>(mem_realloc(vm, _buffer, (uint32)to_alloc));
			if(!_buffer) {
				mem_free(_buffer);
				RETURN_ERROR_SIMPLE();
			}
			alloc = (uint32)to_alloc;
		}
		// copy s2 to into buffer
		memcpy(_buffer+len, s2, req);
		len += req;
		// NULL terminate the C string
		_buffer[len] = 0;

		// check for separator string
		if(i+1 < n && seplen) {
			memcpy(_buffer+len, sep, seplen);
			len += seplen;
			_buffer[len] = 0;
		}

		++i;
	}

	gravity_string_t * result = gravity_string_new(vm, _buffer, len, alloc);
	RETURN_VALUE(VALUE_FROM_OBJECT(reinterpret_cast<gravity_object_t *>(result)), rindex);
}

static bool list_exec(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
	if((nargs != 2) || !args[1].IsInt()) 
		RETURN_ERROR("An Int value is expected as argument of list_exec.");
	uint32 n = (uint32)args[1].GetInt();
	gravity_list_t * list = gravity_list_new(vm, n);
	if(!list) 
		RETURN_ERROR("Maximum List allocation size reached (%d).", MAX_ALLOCATION);
	for(uint32 i = 0; i<n; ++i) {
		list->array.insert(GravityValue::from_null());
	}
	RETURN_VALUE(VALUE_FROM_OBJECT(reinterpret_cast<gravity_object_t *>(list)), rindex);
}

// MARK: - Map Class -

static bool map_count(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    #pragma unused(vm, nargs)
	gravity_map_t * map = VALUE_AS_MAP(args[0]);
	RETURN_VALUE(GravityValue::from_int(gravity_hash_count(map->hash)), rindex);
}

static bool map_keys(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    #pragma unused(nargs)
	gravity_map_t * map = VALUE_AS_MAP(args[0]);
	uint32 count = gravity_hash_count(map->hash);
	gravity_list_t * list = gravity_list_new(vm, count);
	gravity_hash_iterate(map->hash, map_keys_array, (void*)list);
	RETURN_VALUE(VALUE_FROM_OBJECT(reinterpret_cast<gravity_object_t *>(list)), rindex);
}

#if GRAVITY_MAP_DOTSUGAR
static bool map_load(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    #pragma unused(vm, nargs)
	// called when a map is accessed with dot notation
	// for example:
	// var map = ["key1": 10];
	// return map.key1;
	// in this case (since we override object_load super) try to access
	// class first and if nothing is found access its internal hash table
	gravity_map_t * map = VALUE_AS_MAP(args[0]);
	GravityValue key = args[1];
	if(!key) 
		RETURN_ERROR("Invalid map key.");
	// check class first (so user will not be able to break anything)
	gravity_object_t * obj = (gravity_object_t *)gravity_class_lookup(GravityEnv.P_ClsMap, key);
	if(obj) {
		if(obj->IsClosure()) {
			gravity_closure_t * closure = (gravity_closure_t *)obj;
			if(closure && closure->f) {
				// execute optimized default getter
				if(FUNCTION_ISA_SPECIAL(closure->f)) {
					if(FUNCTION_ISA_GETTER(closure->f)) {
						// returns a function to be executed using the return false trick
						RETURN_CLOSURE(VALUE_FROM_OBJECT(reinterpret_cast<gravity_object_t *>((gravity_closure_t *)closure->f->special[EXEC_TYPE_SPECIAL_GETTER])), rindex);
					}
				}
			}
		}
		RETURN_VALUE(VALUE_FROM_OBJECT(obj), rindex);
	}

	// then check its internal hash
	GravityValue * value = gravity_hash_lookup(map->hash, key);
	RETURN_VALUE((value) ? *value : GravityValue::from_null(), rindex);
}

    #endif

static bool map_loadat(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused(vm, nargs)
	// called when a map is accessed with [] notation
	// for example:
	// var map = ["key1": 10];
	// return map["key1"];
	// in this case ALWAYS access its internal hash table
	gravity_map_t * map = VALUE_AS_MAP(args[0]);
	GravityValue key = args[1];
	if(!key) 
		RETURN_ERROR("Invalid map key.");
	GravityValue * value = gravity_hash_lookup(map->hash, key);
	RETURN_VALUE((value) ? *value : GravityValue::from_null(), rindex);
}

static bool map_haskey(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused(vm, nargs)
	gravity_map_t * map = VALUE_AS_MAP(args[0]);
	GravityValue key = args[1];
	GravityValue * value = gravity_hash_lookup(map->hash, key);
	RETURN_VALUE((value) ? VALUE_FROM_TRUE : VALUE_FROM_FALSE, rindex);
}

static bool map_storeat(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused(vm, nargs, rindex)
	gravity_map_t * map = VALUE_AS_MAP(args[0]);
	GravityValue key = args[1];
	GravityValue value = args[2];
	gravity_hash_insert(map->hash, key, value);
	RETURN_NOVALUE();
}

static bool map_remove(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused(vm, nargs)
	gravity_map_t * map = VALUE_AS_MAP(args[0]);
	GravityValue key = args[1];
	bool existed = gravity_hash_remove(map->hash, key);
	RETURN_VALUE(VALUE_FROM_BOOL(existed), rindex);
}

static bool map_loop(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
	if(nargs < 2) 
		RETURN_ERROR("Incorrect number of arguments.");
	if(!args[1].IsClosure()) 
		RETURN_ERROR("Argument must be a Closure.");
	gravity_closure_t * closure = VALUE_AS_CLOSURE(args[1]); // closure to execute
	GravityValue value = args[0];                        // self parameter
	gravity_map_t * map = VALUE_AS_MAP(args[0]);
	uint   n = gravity_hash_count(map->hash);    // times to execute the loop
	uint   i = 0;
	// build keys array
	// do not transfer newlist to GC because it could be freed during closure execution
	gravity_list_t * list = gravity_list_new(NULL, (uint32)n);
	gravity_hash_iterate(map->hash, map_keys_array, (void*)list);
	nanotime_t t1 = nanotime();
	while(i < n) {
		if(!gravity_vm_runclosure(vm, closure, value, &list->array.at(i), 1)) 
			return false;
		++i;
	}
	nanotime_t t2 = nanotime();
	gravity_vm_transfer(vm, (gravity_object_t *)list);
	RETURN_VALUE(GravityValue::from_int(t2-t1), rindex);
}

static bool map_iterator(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused(vm, args, nargs)
	// fix for a bug encountered in the following code
	// var r = ["k1": 123, "k2": 142];
	// for (var data in r) {}
	// the for loop will result in an infinite loop
	// because the special ITERATOR_INIT_FUNCTION key
	// will result in a NULL value (not FALSE)
	RETURN_VALUE(VALUE_FROM_FALSE, rindex);
}

// MARK: - Range Class -

static bool range_count(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused(vm, nargs)
	gravity_range_t * range = VALUE_AS_RANGE(args[0]);
	gravity_int_t count = (range->to > range->from) ? (range->to - range->from) : (range->from - range->to);
	RETURN_VALUE(GravityValue::from_int(count+1), rindex);
}

static bool range_from(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused(vm, nargs)
	gravity_range_t * range = VALUE_AS_RANGE(args[0]);
	RETURN_VALUE(GravityValue::from_int(range->from), rindex);
}

static bool range_to(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused(vm, nargs)
	gravity_range_t * range = VALUE_AS_RANGE(args[0]);
	RETURN_VALUE(GravityValue::from_int(range->to), rindex);
}

static bool range_loop(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
	if(nargs < 2) 
		RETURN_ERROR("Incorrect number of arguments.");
	if(!args[1].IsClosure()) 
		RETURN_ERROR("Argument must be a Closure.");
	gravity_closure_t * closure = VALUE_AS_CLOSURE(args[1]); // closure to execute
	GravityValue value = args[0];                       // self parameter
	gravity_range_t * range = VALUE_AS_RANGE(value);
	bool is_forward = range->from < range->to;
	nanotime_t t1 = nanotime();
	if(is_forward) {
		gravity_int_t n = range->to;
		gravity_int_t i = range->from;
		while(i <= n) {
			if(!gravity_vm_runclosure(vm, closure, value, &GravityValue::from_int(i), 1)) 
				return false;
			++i;
		}
	}
	else {
		gravity_int_t n = range->from;    // 5...1
		gravity_int_t i = range->to;
		while(n >= i) {
			if(!gravity_vm_runclosure(vm, closure, value, &GravityValue::from_int(n), 1)) 
				return false;
			--n;
		}
	}
	nanotime_t t2 = nanotime();
	RETURN_VALUE(GravityValue::from_int(t2-t1), rindex);
}

static bool range_iterator(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    #pragma unused(vm, nargs)
	gravity_range_t * range = VALUE_AS_RANGE(args[0]);
	if(range->to < range->from)  // check for invalid range first
		RETURN_VALUE(VALUE_FROM_FALSE, rindex);
	if(VALUE_ISA_NULL(args[1]))  // check for start of iteration
		RETURN_VALUE(GravityValue::from_int(range->from), rindex);
	GravityValue value = args[1]; // extract value
	if(!value.IsInt())  // check error condition
		RETURN_ERROR("Iterator expects a numeric value here.");
	// compute new value
	gravity_int_t n = value.n;
	if(range->from < range->to) {
		++n;
		if(n > range->to) 
			RETURN_VALUE(VALUE_FROM_FALSE, rindex);
	}
	else {
		--n;
		if(n < range->to) 
			RETURN_VALUE(VALUE_FROM_FALSE, rindex);
	}
	RETURN_VALUE(GravityValue::from_int(n), rindex); // return new iterator
}

static bool range_iterator_next(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused(vm, nargs)
	RETURN_VALUE(args[1], rindex);
}

static bool range_contains(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused(vm, nargs)
	gravity_range_t * range = VALUE_AS_RANGE(args[0]);
	GravityValue value = args[1];
	// check error condition
	if(!value.IsInt()) 
		RETURN_ERROR("A numeric value is expected.");
	RETURN_VALUE(VALUE_FROM_BOOL((value.n >= range->from) && (value.n <= range->to)), rindex);
}

static bool range_exec(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
	if((nargs != 3) || !args[1].IsInt() || !args[2].IsInt()) 
		RETURN_ERROR("Two Int values are expected as argument of Range creation.");
	uint32 n1 = (uint32)args[1].GetInt();
	uint32 n2 = (uint32)args[2].GetInt();
	gravity_range_t * range = gravity_range_new(vm, n1, n2, true);
	RETURN_VALUE(VALUE_FROM_OBJECT(reinterpret_cast<gravity_object_t *>(range)), rindex);
}

// MARK: - Class Class -

static bool class_name(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused(vm, nargs)
	gravity_class_t * c = (args[0].p);
	RETURN_VALUE(VALUE_FROM_CSTRING(vm, c->identifier), rindex);
}

static bool class_exec(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
	if(args[0].IsClass()) {
		gravity_class_t * c = (gravity_class_t *)args[0].p;
		if(gravity_iscore_class(c)) {
			STATICVALUE_FROM_STRING(exec_key, GRAVITY_INTERNAL_EXEC_NAME, strlen(GRAVITY_INTERNAL_EXEC_NAME));
			gravity_closure_t * closure = gravity_class_lookup_closure(c, exec_key);
			if(closure) 
				RETURN_CLOSURE(VALUE_FROM_OBJECT(reinterpret_cast<gravity_object_t *>(closure)), rindex);
		}
	}
	// if 1st argument is not a class that means that this execution is part of a inner classes chained init
	// so retrieve class from callable object (that I am sure it is the right class)
	// this is more an hack than an elegation solution, I really hope to find out a better way
	if(!args[0].IsClass()) 
		args[0] = *(args-1);
	// retrieve class (with sanity check)
	if(!args[0].IsClass()) 
		RETURN_ERROR("Unable to execute non class object.");
	gravity_class_t * c = (gravity_class_t *)args[0].p;
	// perform alloc (then check for init)
	gravity_gc_setenabled(vm, false);
	gravity_instance_t * instance = gravity_instance_new(vm, c);
	// special case: check if superclass is an xdata class
	gravity_delegate_t * delegate = gravity_vm_delegate(vm);
	if(c->superclass->xdata && delegate->bridge_initinstance) {
		delegate->bridge_initinstance(vm, c->superclass->xdata, GravityValue::from_null(), instance, NULL, 1);
	}

	// if is inner class then ivar 0 is reserved for a reference to its outer class
	if(c->has_outer) gravity_instance_setivar(instance, 0, gravity_vm_getslot(vm, 0));

	// check for constructor function (-1 because self implicit parameter does not count)
	gravity_closure_t * closure = (gravity_closure_t *)gravity_class_lookup_constructor(c, nargs-1);

	// replace first parameter (self) to newly allocated instance
	args[0] = VALUE_FROM_OBJECT(reinterpret_cast<gravity_object_t *>(instance));

	// if constructor found in this class then executes it
	if(closure) {
		// as with func call even in constructor if less arguments are passed then fill the holes with UNDEFINED
		// values
		if(nargs < closure->f->nparams) {
			uint16 rargs = nargs;
			while(rargs < closure->f->nparams) {
				args[rargs] = GravityValue::from_undefined();
				++rargs;
			}
		}
		gravity_gc_setenabled(vm, true);
		RETURN_CLOSURE(VALUE_FROM_OBJECT(reinterpret_cast<gravity_object_t *>(closure)), rindex);
	}

	// no closure found (means no constructor found in this class)
	if(c->xdata && delegate->bridge_initinstance) {
		// even if no closure is found try to execute the default bridge init instance (if class is bridged)
		if(nargs != 1) 
			RETURN_ERROR("No init with %d parameters found in class %s", nargs-1, c->identifier);
		delegate->bridge_initinstance(vm, c->xdata, args[0], instance, args, nargs);
	}
	gravity_gc_setenabled(vm, true);
	// in any case set destination register to newly allocated instance
	RETURN_VALUE(VALUE_FROM_OBJECT(reinterpret_cast<gravity_object_t *>(instance)), rindex);
}

// MARK: - Closure Class -

static bool closure_disassemble(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    #pragma unused(nargs)
	gravity_closure_t * closure = (gravity_closure_t *)(args[0].p);
	if(closure->f->tag != EXEC_TYPE_NATIVE) RETURN_VALUE(GravityValue::from_null(), rindex);
	const char * buffer = gravity_disassemble(vm, closure->f, (const char*)closure->f->bytecode, closure->f->ninsts, false);
	if(!buffer) RETURN_VALUE(GravityValue::from_null(), rindex);
	RETURN_VALUE(gravity_string_to_value(vm, buffer, AUTOLENGTH), rindex);
}

static bool closure_apply(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
	if(nargs != 3) 
		RETURN_ERROR("Two arguments are needed by the apply function.");
	if(!args[2].IsList()) 
		RETURN_ERROR("A list of arguments is required in the apply function.");
	gravity_closure_t * closure = VALUE_AS_CLOSURE(args[0]);
	GravityValue self_value = args[1];
	gravity_list_t * list = static_cast<gravity_list_t *>(args[2]);
	if(!gravity_vm_runclosure(vm, closure, self_value, list->array.p, (uint16)list->array.getCount())) 
		return false;
	GravityValue result = gravity_vm_result(vm);
	RETURN_VALUE(result, rindex);
}

static bool closure_bind(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
	if(nargs != 2) 
		RETURN_ERROR("An argument is required by the bind function.");
	if(!args[0].IsClosure()) {
		// Houston, we have a problem
		RETURN_NOVALUE();
	}
	gravity_closure_t * closure = VALUE_AS_CLOSURE(args[0]);
	if(VALUE_ISA_NULL(args[1])) {
		closure->context = NULL;
	}
	else if(args[1].IsObject()) {
		closure->context = VALUE_AS_OBJECT(args[1]);
	}
	RETURN_NOVALUE();
}

// MARK: - Function Class -

static bool function_closure(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    #pragma unused(nargs)
	gravity_function_t * func = VALUE_AS_FUNCTION(args[0]);
	gravity_closure_t * closure = gravity_closure_new(vm, func);
	RETURN_VALUE(VALUE_FROM_OBJECT(reinterpret_cast<gravity_object_t *>(closure)), rindex);
}

static bool function_exec(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
	if(!args[0].IsFunction()) 
		RETURN_ERROR("Unable to convert Object to closure");
	gravity_function_t * func = VALUE_AS_FUNCTION(args[0]);
	gravity_closure_t * closure = gravity_closure_new(vm, func);

	// fix the default arg values case
	// to be really correct and safe, stack size should be checked here but I know in advance that a minimum
	// DEFAULT_MINSTACK_SIZE
	// is guaratee before calling a function, so just make sure that you do not use more than DEFAULT_MINSTACK_SIZE
	// arguments
	// DEFAULT_MINSTACK_SIZE is 256 and in case of a 256 function arguments a maximum registers error would be
	// returned
	// so I can assume to be always safe here
	while(nargs < func->nparams) {
		uint32 index = (func->nparams - nargs);
		args[index] = GravityValue::from_undefined();
		++nargs;
	}

	RETURN_CLOSURE(VALUE_FROM_OBJECT(reinterpret_cast<gravity_object_t *>(closure)), rindex);
}

// MARK: - Float Class -

static bool operator_float_add(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused(vm, nargs)
	GravityValue v1 = args[0];
	GravityValue v2 = args[1];
	INTERNAL_CONVERT_FLOAT(v1, true);
	INTERNAL_CONVERT_FLOAT(v2, true);
	RETURN_VALUE(GravityValue::from_float(v1.f + v2.f), rindex);
}

static bool operator_float_sub(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused(vm, nargs)
	GravityValue v1 = args[0];
	GravityValue v2 = args[1];	
	INTERNAL_CONVERT_FLOAT(v1, true);
	INTERNAL_CONVERT_FLOAT(v2, true);
	RETURN_VALUE(GravityValue::from_float(v1.f - v2.f), rindex);
}

static bool operator_float_div(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused(vm, nargs)
	GravityValue v1 = args[0];
	GravityValue v2 = args[1];	
	INTERNAL_CONVERT_FLOAT(v1, true);
	INTERNAL_CONVERT_FLOAT(v2, true);
	if(v2.f == 0.0) 
		RETURN_ERROR("Division by 0 error.");
	RETURN_VALUE(GravityValue::from_float(v1.f / v2.f), rindex);
}

static bool operator_float_mul(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused(vm, nargs)
	GravityValue v1 = args[0];
	GravityValue v2 = args[1];	
	INTERNAL_CONVERT_FLOAT(v1, true);
	INTERNAL_CONVERT_FLOAT(v2, true);
	RETURN_VALUE(GravityValue::from_float(v1.f * v2.f), rindex);
}

static bool operator_float_rem(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused(vm, nargs)
	GravityValue v1 = args[0];
	GravityValue v2 = args[1];
	INTERNAL_CONVERT_FLOAT(v1, true);
	INTERNAL_CONVERT_FLOAT(v2, true);
	// compute floating point modulus
    #if GRAVITY_ENABLE_DOUBLE
	RETURN_VALUE(GravityValue::from_float(remainder(v1.f, v2.f)), rindex);
    #else
	RETURN_VALUE(GravityValue::from_float(remainderf(v1.f, v2.f)), rindex);
    #endif
}

static bool operator_float_and(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused(vm, nargs)
	GravityValue v1 = args[0];
	GravityValue v2 = args[1];	
	INTERNAL_CONVERT_BOOL(v1, true);
	INTERNAL_CONVERT_BOOL(v2, true);
	RETURN_VALUE(VALUE_FROM_BOOL(v1.n && v2.n), rindex);
}

static bool operator_float_or(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused(vm, nargs)
	GravityValue v1 = args[0];
	GravityValue v2 = args[1];
	INTERNAL_CONVERT_BOOL(v1, true);
	INTERNAL_CONVERT_BOOL(v2, true);
	RETURN_VALUE(VALUE_FROM_BOOL(v1.n || v2.n), rindex);
}

static bool operator_float_neg(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex)
{
    #pragma unused(vm, nargs)
	RETURN_VALUE(GravityValue::from_float(-args[0].f), rindex);
}

static bool operator_float_not(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    #pragma unused(vm, nargs)
	RETURN_VALUE(VALUE_FROM_BOOL(!args[0].f), rindex);
}

static bool operator_float_cmp(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused(vm, nargs)
	GravityValue v1 = args[0];
	GravityValue v2 = args[1];
	INTERNAL_CONVERT_FLOAT(v2, true);
	// simpler equality test
	if(!!v2) {
		if(v1.f == v2.f) RETURN_VALUE(GravityValue::from_int(0), rindex);
		if(v1.f > v2.f) RETURN_VALUE(GravityValue::from_int(1), rindex);
	}
	RETURN_VALUE(GravityValue::from_int(-1), rindex);
}

static bool function_float_round(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused(vm, nargs)
    #if GRAVITY_ENABLE_DOUBLE
		RETURN_VALUE(GravityValue::from_float(round(args[0].f)), rindex);
    #else
		RETURN_VALUE(GravityValue::from_float(roundf(args[0].f)), rindex);
    #endif
}

static bool function_float_floor(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused(vm, nargs)
    #if GRAVITY_ENABLE_DOUBLE
		RETURN_VALUE(GravityValue::from_float(floor(args[0].f)), rindex);
    #else
		RETURN_VALUE(GravityValue::from_float(floorf(args[0].f)), rindex);
    #endif
}

static bool function_float_ceil(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused(vm, nargs)
    #if GRAVITY_ENABLE_DOUBLE
		RETURN_VALUE(GravityValue::from_float(ceil(args[0].f)), rindex);
    #else
		RETURN_VALUE(GravityValue::from_float(ceilf(args[0].f)), rindex);
    #endif
}

static bool float_exec(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
	if(nargs != 2) 
		RETURN_ERROR("A single argument is expected in Float casting.");
	GravityValue v = convert_value2float(vm, args[1]);
	if(!v) 
		RETURN_ERROR("Unable to convert object to Float.");
	RETURN_VALUE(v, rindex);
}

static bool float_degrees(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused(vm, nargs)
	// Convert the float from radians to degrees
	RETURN_VALUE(GravityValue::from_float(args[0].f*180.0/SMathConst::Pi), rindex);
}

static bool float_radians(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    #pragma unused(vm, nargs)
	// Convert the float from degrees to radians
	RETURN_VALUE(GravityValue::from_float(args[0].f*SMathConst::Pi/180.0), rindex);
}

static bool float_isclose(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
	if(nargs < 2) 
		RETURN_VALUE(VALUE_FROM_BOOL(true), rindex);
	GravityValue v1 = args[0];
	GravityValue v2 = args[1];
	INTERNAL_CONVERT_FLOAT(v2, true);
	gravity_float_t rel_tol = 1e-09;
	gravity_float_t abs_tol = 0.0;
	if(nargs > 2 && args[2].IsFloat()) 
		rel_tol = args[2].GetFloat();
	if(nargs > 3 && args[3].IsFloat()) 
		abs_tol = args[3].GetFloat();
	// abs(a-b) <= max(rel_tol * max(abs(a), abs(b)), abs_tol)
    #if GRAVITY_ENABLE_DOUBLE
	gravity_float_t abs_diff = fabs(v1.f - v2.f);
	gravity_float_t abs_a = fabs(v1.f);
	gravity_float_t abs_b = fabs(v2.f);
	gravity_float_t abs_max = fmax(abs_a, abs_b);
	gravity_float_t result = fmax(rel_tol * abs_max, abs_tol);
    #else
	gravity_float_t abs_diff = fabsf(v1.f - v2.f);
	gravity_float_t abs_a = fabsf(v1.f);
	gravity_float_t abs_b = fabsf(v2.f);
	gravity_float_t abs_max = fmaxf(abs_a, abs_b);
	gravity_float_t result = fmaxf(rel_tol * abs_max, abs_tol);
    #endif

	RETURN_VALUE(VALUE_FROM_BOOL(abs_diff <= result), rindex);
}

static bool float_min(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    #pragma unused(vm, args, nargs)
	RETURN_VALUE(GravityValue::from_float(GRAVITY_FLOAT_MIN), rindex);
}

static bool float_max(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    #pragma unused(vm, args, nargs)
	RETURN_VALUE(GravityValue::from_float(GRAVITY_FLOAT_MAX), rindex);
}

// MARK: - Int Class -

// binary operators
static bool operator_int_add(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex)
{
    //#pragma unused (nargs)
	GravityValue v1 = args[0];
	GravityValue v2 = args[1];
	INTERNAL_CONVERT_INT(v2, true);
	RETURN_VALUE(GravityValue::from_int(v1.n + v2.n), rindex);
}

static bool operator_int_sub(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused (nargs)
	GravityValue v1 = args[0];
	GravityValue v2 = args[1];
	INTERNAL_CONVERT_INT(v2, true);
	RETURN_VALUE(GravityValue::from_int(v1.n - v2.n), rindex);
}

static bool operator_int_div(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused (nargs)
	GravityValue v1 = args[0];
	GravityValue v2 = args[1];
	INTERNAL_CONVERT_INT(v2, true);
	if(v2.n == 0) 
		RETURN_ERROR("Division by 0 error.");
	RETURN_VALUE(GravityValue::from_int(v1.n / v2.n), rindex);
}

static bool operator_int_mul(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused (nargs)
	GravityValue v1 = args[0];
	GravityValue v2 = args[1];
	INTERNAL_CONVERT_INT(v2, true);
	RETURN_VALUE(GravityValue::from_int(v1.n * v2.n), rindex);
}

static bool operator_int_rem(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused (nargs)
	GravityValue v1 = args[0];
	GravityValue v2 = args[1];
	INTERNAL_CONVERT_INT(v2, true);
	if(v2.n == 0) 
		RETURN_ERROR("Reminder by 0 error.");
	RETURN_VALUE(GravityValue::from_int(v1.n % v2.n), rindex);
}

static bool operator_int_and(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused (nargs)
	GravityValue v1 = args[0];
	GravityValue v2 = args[1];
	INTERNAL_CONVERT_BOOL(v1, true);
	INTERNAL_CONVERT_BOOL(v2, true);
	RETURN_VALUE(VALUE_FROM_BOOL(v1.n && v2.n), rindex);
}

static bool operator_int_or(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused (nargs)
	GravityValue v1 = args[0];
	GravityValue v2 = args[1];
	INTERNAL_CONVERT_BOOL(v1, true);
	INTERNAL_CONVERT_BOOL(v2, true);
	RETURN_VALUE(VALUE_FROM_BOOL(v1.n || v2.n), rindex);
}

static bool operator_int_neg(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused(vm, nargs)
	RETURN_VALUE(GravityValue::from_int(-args[0].n), rindex);
}

static bool operator_int_not(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused(vm, nargs)
	RETURN_VALUE(VALUE_FROM_BOOL(!args[0].n), rindex);
}

static bool operator_int_cmp(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
	if(args[1].IsFloat()) {
		// args[0] is INT and args[1] is FLOAT, in this case
		// args[0] must be manually converted to FLOAT before the call
		args[0] = GravityValue::from_float((gravity_float_t)args[0].n);
		return operator_float_cmp(vm, args, nargs, rindex);
	}
	GravityValue v1 = args[0];
	GravityValue v2 = args[1];
	INTERNAL_CONVERT_INT(v2, true);
	if(!!v2) {
		if(v1.n == v2.n) RETURN_VALUE(GravityValue::from_int(0), rindex);
		if(v1.n > v2.n) RETURN_VALUE(GravityValue::from_int(1), rindex);
	}
	RETURN_VALUE(GravityValue::from_int(-1), rindex);
}

static bool int_loop(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
	if(nargs < 2) 
		RETURN_ERROR("Incorrect number of arguments.");
	if(!args[1].IsClosure()) 
		RETURN_ERROR("Argument must be a Closure.");
	gravity_closure_t * closure = VALUE_AS_CLOSURE(args[1]); // closure to execute
	GravityValue value = args[0];                       // self parameter
	gravity_int_t n = value.n;                         // times to execute the loop
	gravity_int_t i = 0;
	nanotime_t t1 = nanotime();
	while(i < n) {
		if(!gravity_vm_runclosure(vm, closure, value, &GravityValue::from_int(i), 1)) 
			return false;
		++i;
	}
	nanotime_t t2 = nanotime();
	RETURN_VALUE(GravityValue::from_int(t2-t1), rindex);
}

static bool int_random(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    #pragma unused(args)
	if(nargs != 3) 
		RETURN_ERROR("Int.random() expects 2 integer arguments");
	if(!args[1].IsInt() || !args[2].IsInt()) 
		RETURN_ERROR("Int.random() arguments must be integers");
	gravity_int_t num1 = args[1].GetInt();
	gravity_int_t num2 = args[2].GetInt();
	// Only Seed once
	static bool already_seeded = false;
	if(!already_seeded) {
		srand((unsigned)time(NULL));
		already_seeded = true;
	}
	int r;
	// if num1 is lower, consider it min, otherwise, num2 is min
	if(num1 < num2) {
		// returns a random integer between num1 and num2 inclusive
		r = (int)((rand() % (num2 - num1 + 1)) + num1);
	}
	else if(num1 > num2) {
		r = (int)((rand() % (num1 - num2 + 1)) + num2);
	}
	else {
		r = (int)num1;
	}
	RETURN_VALUE(GravityValue::from_int(r), rindex);
}

static bool int_exec(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
	if(nargs != 2) 
		RETURN_ERROR("A single argument is expected in Int casting.");
	GravityValue v = convert_value2int(vm, args[1]);
	if(!v) 
		RETURN_ERROR("Unable to convert object to Int.");
	RETURN_VALUE(v, rindex);
}

static bool int_degrees(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    #pragma unused(vm, nargs)
	// Convert the int from radians to degrees
	RETURN_VALUE(GravityValue::from_float(args[0].n * 180.0 / SMathConst::Pi), rindex);
}

static bool int_radians(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    #pragma unused(vm, nargs)
	// Convert the int from degrees to radians
	RETURN_VALUE(GravityValue::from_float(args[0].n * SMathConst::Pi / 180.0), rindex);
}

static bool int_min(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    #pragma unused(vm, args, nargs)
	RETURN_VALUE(GravityValue::from_int(GRAVITY_INT_MIN), rindex);
}

static bool int_max(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    #pragma unused(vm, args, nargs)
	RETURN_VALUE(GravityValue::from_int(GRAVITY_INT_MAX), rindex);
}

// MARK: - Bool Class -

static bool operator_bool_add(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex)  { return operator_int_add(vm, args, nargs, rindex); }
static bool operator_bool_sub(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) { return operator_int_sub(vm, args, nargs, rindex); }
static bool operator_bool_div(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) { return operator_int_div(vm, args, nargs, rindex); }
static bool operator_bool_mul(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) { return operator_int_mul(vm, args, nargs, rindex); }
static bool operator_bool_rem(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) { return operator_int_rem(vm, args, nargs, rindex); }

static bool operator_bool_and(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused(vm, nargs)
	GravityValue v1 = args[0];
	GravityValue v2 = args[1];
	INTERNAL_CONVERT_BOOL(v1, true);
	RETURN_VALUE(VALUE_FROM_BOOL(v1.n && v2.n), rindex);
}

static bool operator_bool_or(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused(vm, nargs)
	GravityValue v1 = args[0];
	GravityValue v2 = args[1];
	INTERNAL_CONVERT_BOOL(v1, true);
	RETURN_VALUE(VALUE_FROM_BOOL(v1.n || v2.n), rindex);
}

static bool operator_bool_bitor(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused(vm, nargs)
	GravityValue v1 = args[0];
	GravityValue v2 = args[1];
	INTERNAL_CONVERT_BOOL(v1, true);
	RETURN_VALUE(VALUE_FROM_BOOL(v1.n | v2.n), rindex);
}

static bool operator_bool_bitand(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused(vm, nargs)
	GravityValue v1 = args[0];
	GravityValue v2 = args[1];
	INTERNAL_CONVERT_BOOL(v1, true);
	RETURN_VALUE(VALUE_FROM_BOOL(v1.n & v2.n), rindex);
}

static bool operator_bool_bitxor(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused(vm, nargs)
	GravityValue v1 = args[0];
	GravityValue v2 = args[1];
	INTERNAL_CONVERT_BOOL(v1, true);
	RETURN_VALUE(VALUE_FROM_BOOL(v1.n ^ v2.n), rindex);
}

static bool operator_bool_cmp(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) {
	return operator_int_cmp(vm, args, nargs, rindex);
}

// unary operators
static bool operator_bool_neg(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) {
    #pragma unused(vm, nargs)
	RETURN_VALUE(GravityValue::from_int(-args[0].n), rindex);
}

static bool operator_bool_not(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused(vm, nargs)
	RETURN_VALUE(GravityValue::from_int(!args[0].n), rindex);
}

static bool bool_exec(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
	if(nargs != 2) 
		RETURN_ERROR("A single argument is expected in Bool casting.");
	GravityValue v = convert_value2bool(vm, args[1]);
	if(!v) 
		RETURN_ERROR("Unable to convert object to Bool.");
	RETURN_VALUE(v, rindex);
}

// MARK: - String Class -

static bool operator_string_add(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused(vm, nargs)
	GravityValue v1 = args[0];
	GravityValue v2 = args[1];	
	INTERNAL_CONVERT_STRING(v2, true);
	gravity_string_t * s1 = static_cast<gravity_string_t *>(v1);
	gravity_string_t * s2 = static_cast<gravity_string_t *>(v2);
	uint32 len = s1->len + s2->len;
	char buffer[4096];
	char * s = NULL;
	// check if I can save an allocation
	if(len+1 < sizeof(buffer)) 
		s = buffer;
	else {
		s = (char *)mem_alloc(vm, len+1);
		CHECK_MEM_ALLOC(s);
	}
	memcpy(s, s1->s, s1->len);
	memcpy(s+s1->len, s2->s, s2->len);
	GravityValue v = VALUE_FROM_STRING(vm, s, len);
	if(s != NULL && s != buffer) 
		mem_free(s);
	RETURN_VALUE(v, rindex);
}

static bool operator_string_sub(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused(vm, nargs)
	GravityValue v1 = args[0];
	GravityValue v2 = args[1];
	INTERNAL_CONVERT_STRING(v2, true);
	gravity_string_t * s1 = static_cast<gravity_string_t *>(v1);
	gravity_string_t * s2 = static_cast<gravity_string_t *>(v2);
	// special case
	if(s2->len == 0) {
		RETURN_VALUE(VALUE_FROM_CSTRING(vm, s1->s), rindex);
	}
	// subtract s2 from s1
	char * found = string_strnstr(s1->s, s2->s, (size_t)s1->len);
	if(!found) 
		RETURN_VALUE(VALUE_FROM_STRING(vm, s1->s, s1->len), rindex);
	// substring found
	// now check if entire substring must be considered
	uint32 flen = (uint32)strlen(found);
	if(flen == s2->len) 
		RETURN_VALUE(VALUE_FROM_STRING(vm, s1->s, (uint32)(found - s1->s)), rindex);
	// sanity check for malformed strings
	if(flen < s2->len) 
		RETURN_ERROR("Malformed string.");
	// substring found but cannot be entirely considered
	uint32 alloc = MAX(s1->len + s2->len +1, DEFAULT_MINSTRING_SIZE);
	char * s = static_cast<char *>(mem_alloc(vm, alloc));
	CHECK_MEM_ALLOC(s);
	uint32 seek = (uint32)(found - s1->s);
	uint32 len = seek + (flen - s2->len);
	memcpy(s, s1->s, seek);
	memcpy(s+seek, found+s2->len, flen - s2->len);
		gravity_string_t * string = gravity_string_new(vm, s, len, alloc);
	RETURN_VALUE(VALUE_FROM_OBJECT(reinterpret_cast<gravity_object_t *>(string)), rindex);
}

static bool operator_string_and(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex)
{
    //#pragma unused(vm, nargs)
	GravityValue v1 = args[0];
	GravityValue v2 = args[1];
	INTERNAL_CONVERT_BOOL(v1, true);
	INTERNAL_CONVERT_BOOL(v2, true);
	RETURN_VALUE(VALUE_FROM_BOOL(v1.n && v2.n), rindex);
}

static bool operator_string_or(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused(vm, nargs)
	GravityValue v1 = args[0];
	GravityValue v2 = args[1];
	INTERNAL_CONVERT_BOOL(v1, true);
	INTERNAL_CONVERT_BOOL(v2, true);
	RETURN_VALUE(VALUE_FROM_BOOL(v1.n || v2.n), rindex);
}

static bool operator_string_neg(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused(vm, nargs)
	GravityValue v1 = args[0];
	// reverse the string
	gravity_string_t * s1 = static_cast<gravity_string_t *>(v1);
	char * s = (char *)string_ndup(s1->s, s1->len);
	if(!utf8_reverse(s)) 
		RETURN_ERROR("Unable to reverse a malformed string.");
	gravity_string_t * string = gravity_string_new(vm, s, s1->len, s1->len);
	RETURN_VALUE(VALUE_FROM_OBJECT(reinterpret_cast<gravity_object_t*>(string)), rindex);
}

static bool operator_string_cmp(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused(vm, nargs)
	GravityValue v1 = args[0];
	GravityValue v2 = args[1];
	INTERNAL_CONVERT_STRING(v2, true);
	if(!!v2) {
		gravity_string_t * s1 = static_cast<gravity_string_t *>(v1);
		gravity_string_t * s2 = static_cast<gravity_string_t *>(v2);
		RETURN_VALUE(GravityValue::from_int(strcmp(s1->s, s2->s)), rindex);
	}
	RETURN_VALUE(GravityValue::from_int(-1), rindex);
}

static bool string_bytes(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused(vm, nargs)
	GravityValue v1 = args[0];
	gravity_string_t * s1 = static_cast<gravity_string_t *>(v1);
	RETURN_VALUE(GravityValue::from_int(s1->len), rindex);
}

static bool string_length(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused(vm, nargs)
	GravityValue v1 = args[0];
	gravity_string_t * s1 = static_cast<gravity_string_t *>(v1);
	uint32 length = (s1->len) ? utf8_len(s1->s, s1->len) : 0;
	RETURN_VALUE(GravityValue::from_int(length), rindex);
}

static bool string_index(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused(vm)
	if((nargs != 2) || (!args[1].IsString())) {
		RETURN_ERROR("String.index() expects a string as an argument");
	}
	gravity_string_t * main_str = static_cast<gravity_string_t *>(args[0]);
	gravity_string_t * str_to_index = static_cast<gravity_string_t *>(args[1]);
	// search for the string
	char * ptr = string_strnstr(main_str->s, str_to_index->s, (size_t)main_str->len);
	// if it doesn't exist, return null
	if(ptr == NULL) {
		RETURN_VALUE(GravityValue::from_null(), rindex);
	}
	// otherwise, return the difference, which is the index that the string starts at
	else {
		RETURN_VALUE(GravityValue::from_int(ptr - main_str->s), rindex);
	}
}

static bool string_contains(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    #pragma unused(vm)
	// sanity check
	if((nargs != 2) || (!args[1].IsString())) {
		RETURN_ERROR("String.index() expects a string as an argument");
	}
	gravity_string_t * main_str = static_cast<gravity_string_t *>(args[0]);
	gravity_string_t * str_to_index = static_cast<gravity_string_t *>(args[1]);
	// search for the string
	char * ptr = string_strnstr(main_str->s, str_to_index->s, (size_t)main_str->len);
	// return a Bool value
	RETURN_VALUE(VALUE_FROM_BOOL(ptr != NULL), rindex);
}

static bool string_count(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    #pragma unused(vm)
	if((nargs != 2) || !args[1].IsString()) {
		RETURN_ERROR("String.count() expects a string as an argument");
	}
	gravity_string_t * main_str = static_cast<gravity_string_t *>(args[0]);
	gravity_string_t * str_to_count = static_cast<gravity_string_t *>(args[1]);
	int j = 0;
	int count = 0;
	// iterate through whole string
	for(uint32 i = 0; i < main_str->len; ++i) {
		if(main_str->s[i] == str_to_count->s[j]) {
			// if the characters match and we are on the last character of the search
			// string, then we have found a match
			if(j == str_to_count->len - 1) {
				++count;
				j = 0;
				continue;
			}
		}
		// reset if it isn't a match
		else {
			j = 0;
			continue;
		}
		// move forward in the search string if we found a match but we aren't
		// finished checking all the characters of the search string yet
		++j;
	}
	RETURN_VALUE(GravityValue::from_int(count), rindex);
}

static bool string_repeat(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
	if((nargs != 2) || !args[1].IsInt()) {
		RETURN_ERROR("String.repeat() expects an integer argument");
	}
	gravity_string_t * main_str = static_cast<gravity_string_t *>(args[0]);
	gravity_int_t times_to_repeat = args[1].GetInt();
	if(times_to_repeat < 1 || times_to_repeat > MAX_ALLOCATION) {
		RETURN_ERROR("String.repeat() expects a value >= 1 and < %d", MAX_ALLOCATION);
	}

	// figure out the size of the array we need to make to hold the new string
	uint32 new_size = (uint32)(main_str->len * times_to_repeat);
	char * new_str = static_cast<char *>(mem_alloc(vm, new_size+1));
	CHECK_MEM_ALLOC(new_str);

	uint32 seek = 0;
	for(uint32 i = 0; i < times_to_repeat; ++i) {
		memcpy(new_str+seek, main_str->s, main_str->len);
		seek += main_str->len;
	}

	gravity_string_t * s = gravity_string_new(vm, new_str, new_size, new_size);
	RETURN_VALUE(VALUE_FROM_OBJECT(reinterpret_cast<gravity_object_t *>(s)), rindex);
}

static bool string_number(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    #pragma unused(nargs)
	GravityValue value = convert_string2number(static_cast<gravity_string_t *>(args[0]), number_format_any);
	RETURN_VALUE(value, rindex);
}

static bool string_upper(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
	gravity_string_t * main_str = static_cast<gravity_string_t *>(args[0]);
	char * ret = static_cast<char *>(mem_alloc(vm, main_str->len + 1));
	CHECK_MEM_ALLOC(ret);
	strcpy(ret, main_str->s);
	// if no arguments passed, change the whole string to uppercase
	if(nargs == 1) {
		for(uint i = 0; i <= main_str->len; ++i) {
			ret[i] = toupper(ret[i]);
		}
	}
	// otherwise, evaluate all the arguments
	else {
		for(uint16 i = 1; i < nargs; ++i) {
			GravityValue value = args[i];
			if(value.IsInt()) {
				int32 index = (int32)value.GetInt();
				if(index < 0) 
					index = main_str->len + index;
				if((index < 0) || ((uint32)index >= main_str->len)) {
					mem_free(ret);
					RETURN_ERROR("Out of bounds error: index %d beyond bounds 0...%d", index, main_str->len - 1);
				}
				ret[index] = toupper(ret[index]);
			}
			else {
				mem_free(ret);
				RETURN_ERROR("upper() expects either no arguments, or integer arguments.");
			}
		}
	}
	gravity_string_t * s = gravity_string_new(vm, ret, main_str->len, 0);
	RETURN_VALUE(VALUE_FROM_OBJECT(reinterpret_cast<gravity_object_t *>(s)), rindex);
}

static bool string_lower(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
	gravity_string_t * main_str = static_cast<gravity_string_t *>(args[0]);
	char * ret = static_cast<char *>(mem_alloc(vm, main_str->len + 1));
	CHECK_MEM_ALLOC(ret);
	strcpy(ret, main_str->s);
	// if no arguments passed, change the whole string to lowercase
	if(nargs == 1) {
		for(uint i = 0; i <= main_str->len; ++i) {
			ret[i] = tolower(ret[i]);
		}
	}
	// otherwise, evaluate all the arguments
	else {
		for(uint16 i = 1; i < nargs; ++i) {
			GravityValue value = args[i];
			if(value.IsInt()) {
				int32 index = (int32)value.GetInt();
				if(index < 0) index = main_str->len + index;
				if((index < 0) || ((uint32)index >= main_str->len)) {
					mem_free(ret);
					RETURN_ERROR("Out of bounds error: index %d beyond bounds 0...%d", index, main_str->len - 1);
				}

				ret[index] = tolower(ret[index]);
			}
			else {
				mem_free(ret);
				RETURN_ERROR("lower() expects either no arguments, or integer arguments.");
			}
		}
	}
	gravity_string_t * s = gravity_string_new(vm, ret, main_str->len, 0);
	RETURN_VALUE(VALUE_FROM_OBJECT(reinterpret_cast<gravity_object_t *>(s)), rindex);
}

static bool string_loadat(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    #pragma unused(nargs)
	gravity_string_t * string = static_cast<gravity_string_t *>(args[0]);
	GravityValue value = args[1];
	int32 first_index;
	int32 second_index;
	if(value.IsInt()) {
		first_index = (int32)value.GetInt();
		second_index = first_index;
	}
	else if(value.IsRange()) {
		gravity_range_t * range = VALUE_AS_RANGE(value);
		first_index = (int32)range->from;
		second_index = (int32)range->to;
	}
	else {
		RETURN_ERROR("An integer index or index range is required to access string items.");
	}

	if(first_index < 0) first_index = string->len + first_index;
	if((first_index < 0) || ((uint32)first_index >= string->len)) 
		RETURN_ERROR("Out of bounds error: first_index %d beyond bounds 0...%d", first_index, string->len-1);

	if(second_index < 0) second_index = string->len + second_index;
	if((second_index < 0) || ((uint32)second_index >= string->len)) 
		RETURN_ERROR("Out of bounds error: second_index %d beyond bounds 0...%d", second_index, string->len-1);

	uint32 substr_len = first_index < second_index ? second_index - first_index + 1 : first_index - second_index + 1;

	bool is_forward = first_index <= second_index;
	if(!is_forward) {
		char * original = static_cast<char *>(mem_alloc(vm, string->len));
		CHECK_MEM_ALLOC(original);

		// without copying it, we would be modifying the original string
		strncpy((char *)original, string->s, string->len);
		uint32 original_len = (uint32)string->len;

		// Reverse the string, and reverse the indices
		first_index = original_len - first_index -1;

		// reverse the String
		int i = original_len - 1;
		int j = 0;
		char c;
		while(i > j) {
			c = original[i];
			original[i] = original[j];
			original[j] = c;
			--i;
			++j;
		}
		GravityValue s = VALUE_FROM_STRING(vm, original + first_index, substr_len);
		mem_free(original);
		RETURN_VALUE(s, rindex);
	}
	RETURN_VALUE(VALUE_FROM_STRING(vm, string->s + first_index, substr_len), rindex);
}

static bool string_storeat(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    #pragma unused(vm, nargs, rindex)
	gravity_string_t * string = static_cast<gravity_string_t *>(args[0]);
	GravityValue idxvalue = args[1];
	if(!idxvalue.IsInt()) 
		RETURN_ERROR("An integer index is required to access a string item.");
	if(!args[2].IsString()) 
		RETURN_ERROR("A string needs to be assigned to a string index");
	gravity_string_t * value = static_cast<gravity_string_t *>(args[2]);
	int32 index = (int32)idxvalue.GetInt();
	if(index < 0) 
		index = string->len + index;
	if(index < 0 || index >= static_cast<int32>(string->len)) 
		RETURN_ERROR("Out of bounds error: index %d beyond bounds 0...%d", index, string->len-1);
	if(index+value->len - 1 >= string->len) 
		RETURN_ERROR("Out of bounds error: End of inserted string exceeds the length of the initial string");
	// this code is not UTF-8 safe
	for(int i = index; i < static_cast<int>(index+value->len); ++i) {
		string->s[i] = value->s[i-index];
	}
	// characters inside string changed so we need to re-compute hash
	string->hash = gravity_hash_compute_buffer((const char*)string->s, string->len);
	RETURN_NOVALUE();
}

static bool string_split(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
	// sanity check
	if((nargs != 2) || !args[1].IsString()) 
		RETURN_ERROR("String.split() expects 1 string separator.");
	// setup arguments
	gravity_string_t * string = static_cast<gravity_string_t *>(args[0]);
	gravity_string_t * substr = static_cast<gravity_string_t *>(args[1]);
	const char * sep = substr->s;
	uint32 seplen = substr->len;
	// this is a quite complex situation
	// list should not be trasferred to GC bacause it could be freed by VALUE_FROM_STRING
	// and the same applied to each VALUE_FROM_STRING
	// but in order to use NULL as vm parameter I should keep track of each individual allocation here
	// and then transfer all of them to the VM at the end of the loop (in order to be able to have them
	// freed by the GC)
	// I think it is too much work, the easier solution is just to disable GC during execution loop
	gravity_gc_setenabled(vm, false);
	// initialize the list to have a size of 0
	gravity_list_t * list = gravity_list_new(vm, 0);
	char * original = string->s;
	uint32 slen = string->len;
	// if the separator is empty, then we split the string at every character
	if(seplen == 0) {
		for(uint32 i = 0; i<slen;) {
			uint32 n = utf8_charbytes(original, 0);
			list->array.insert(VALUE_FROM_STRING(vm, original, n));
			original += n;
			i += n;
		}
		gravity_gc_setenabled(vm, true);
		RETURN_VALUE(VALUE_FROM_OBJECT(reinterpret_cast<gravity_object_t *>(list)), rindex);
	}

	// split loop
	while(1) {
		char * p = string_strnstr(original, sep, (size_t)slen);
		if(p == NULL) {
			list->array.insert(VALUE_FROM_STRING(vm, original, slen));
			break;
		}
		uint32 vlen = (uint32)(p-original);
		list->array.insert(VALUE_FROM_STRING(vm, original, vlen));
		// update pointer and slen
		original = p + seplen;
		slen -= vlen + seplen;
	}
	gravity_gc_setenabled(vm, true);
	RETURN_VALUE(VALUE_FROM_OBJECT(reinterpret_cast<gravity_object_t *>(list)), rindex);
}

static bool string_find_replace(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
	// sanity check
	if((nargs != 3) || !args[1].IsString() || !args[2].IsString()) 
		RETURN_ERROR("String.replace() expects 2 string arguments.");
	// setup arguments
	gravity_string_t * string = static_cast<gravity_string_t *>(args[0]);
	gravity_string_t * from = static_cast<gravity_string_t *>(args[1]);
	gravity_string_t * to = static_cast<gravity_string_t *>(args[2]);
	size_t len = 0;
	// perform search and replace
	char * s = string_replace(string->s, from->s, to->s, &len);
	// return result
	if(s && len) RETURN_VALUE(VALUE_FROM_STRING(vm, s, (uint32)len), rindex);
	RETURN_VALUE(GravityValue::from_null(), rindex);
}

static bool string_loop(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
	if(nargs < 2) 
		RETURN_ERROR("Incorrect number of arguments.");
	if(!args[1].IsClosure()) 
		RETURN_ERROR("Argument must be a Closure.");
	gravity_closure_t * closure = VALUE_AS_CLOSURE(args[1]); // closure to execute
	GravityValue value = args[0];                        // self parameter
	gravity_string_t * string = static_cast<gravity_string_t *>(value);
	char * str = string->s;
	gravity_int_t n = string->len; // Times to execute the loop
	gravity_int_t i = 0;
	nanotime_t t1 = nanotime();
	while(i < n) {
		GravityValue v_str = VALUE_FROM_STRING(vm, str + i, 1);
		if(!gravity_vm_runclosure(vm, closure, value, &v_str, 1)) 
			return false;
		++i;
	}
	nanotime_t t2 = nanotime();
	RETURN_VALUE(GravityValue::from_int(t2-t1), rindex);
}

static bool string_iterator(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused(vm, nargs)
	gravity_string_t * string = static_cast<gravity_string_t *>(args[0]);
	// check for empty string first
	if(string->len == 0) 
		RETURN_VALUE(VALUE_FROM_FALSE, rindex);
	// check for start of iteration
	if(VALUE_ISA_NULL(args[1])) 
		RETURN_VALUE(GravityValue::from_int(0), rindex);
	// extract value
	GravityValue value = args[1];
	// check error condition
	if(!value.IsInt()) 
		RETURN_ERROR("Iterator expects a numeric value here.");
	// compute new value
	gravity_int_t index = value.n;
	if(index+1 < string->len) {
		uint32 n = utf8_charbytes(string->s + index, 0);
		index += n;
	}
	else {
		RETURN_VALUE(VALUE_FROM_FALSE, rindex);
	}
	// return new iterator
	RETURN_VALUE(GravityValue::from_int(index), rindex);
}

static bool string_iterator_next(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused(vm, nargs)
	gravity_string_t * string = static_cast<gravity_string_t *>(args[0]);
	int32 index = (int32)args[1].GetInt();
	uint32 n = utf8_charbytes(string->s + index, 0);
	RETURN_VALUE(VALUE_FROM_STRING(vm, string->s + index, n), rindex);
}

static bool string_exec(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
	if(nargs != 2) 
		RETURN_ERROR("A single argument is expected in String casting.");
	GravityValue v = convert_value2string(vm, args[1]);
	if(!v) 
		RETURN_ERROR("Unable to convert object to String.");
	RETURN_VALUE(v, rindex);
}

static bool string_trim(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
	int32 direction = 0; // means trim both left and right
	// optional second parameters to specify trim-left (1) or trim-right(2)
	// default is trim-both(0)
	if((nargs == 2) && args[1].IsInt()) {
		int32 v = (int32)args[1].GetInt();
		if(v >= 0 && v <= 2) 
			direction = v;
	}
	gravity_string_t * src = static_cast<gravity_string_t *>(args[0]);
	const char * s = src->s;
	int32 index_left = 0;
	int32 index_right = src->len;
	// process left
	if(oneof2(direction, 0, 1)) {
		for(int32 i = 0; i<index_right; ++i) {
			if(isspace(s[i])) ++index_left;
			else break;
		}
	}
	// process right
	if(oneof2(direction, 0, 2)) {
		for(int32 i = index_right-1; i>=index_left; --i) {
			if(isspace(s[i])) --index_right;
			else break;
		}
	}
	// index_left and index_right now points to the right indexes
	uint32 trim_len = (index_left - 0) + (src->len - index_right);
	GravityValue value = VALUE_FROM_STRING(vm, (const char*)&s[index_left], src->len - trim_len);
	RETURN_VALUE(value, rindex);
}

static bool string_raw(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused(vm, nargs)
	gravity_string_t * string = static_cast<gravity_string_t *>(args[0]);
	uint32 ascii = 0;
	uint32 n = utf8_charbytes(string->s, 0);
	for(uint32 i = 0; i<n; ++i) {
		// if (n > 1) {printf("%u (%d)\n", (uint8_t)string->s[i], (uint32)pow(10, n-(i+1)));}
		ascii += (uint32)((uint8_t)string->s[i] * pow(10, n - (i + 1)));
	}
	RETURN_VALUE(GravityValue::from_int(ascii), rindex);
}

static bool string_toclass(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused(vm, nargs)
	GravityValue result = gravity_vm_lookup(vm, args[0]);
	if(result.IsClass()) 
		RETURN_VALUE(result, rindex);
	RETURN_VALUE(GravityValue::from_null(), rindex);
}

// MARK: - Fiber Class -

static bool fiber_create(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused(nargs)
	if(!args[1].IsClosure()) 
		RETURN_ERROR("A function is expected as argument to Fiber.create.");
	gravity_fiber_t * fiber = gravity_fiber_new(vm, VALUE_AS_CLOSURE(args[1]), 0, 0);
	RETURN_VALUE(VALUE_FROM_OBJECT(reinterpret_cast<gravity_object_t *>(fiber)), rindex);
}

static bool fiber_run(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex, bool is_trying) 
{
    #pragma unused(nargs, rindex)
	// set rindex slot to NULL in order to falsify the if closure check performed by the VM
	gravity_vm_setslot(vm, GravityValue::from_null(), rindex);
	gravity_fiber_t * fiber = VALUE_AS_FIBER(args[0]);
	if(fiber->caller != NULL) 
		RETURN_ERROR("Fiber has already been called.");
	// always update elapsed time
	fiber->elapsedtime = (nanotime() - fiber->lasttime) / 1000000000.0f;
	// check if minimum timewait is passed
	if(fiber->timewait > 0.0f) {
		if(fiber->elapsedtime < fiber->timewait) RETURN_NOVALUE();
	}
	// remember who ran the fiber
	fiber->caller = gravity_vm_fiber(vm);
	// set trying flag
	fiber->trying = is_trying;
	fiber->status = (is_trying) ? FIBER_TRYING : FIBER_RUNNING;
	// switch currently running fiber
	gravity_vm_setfiber(vm, fiber);
	RETURN_FIBER();
}

static bool fiber_exec(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
	return fiber_run(vm, args, nargs, rindex, false);
}

static bool fiber_try(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
	return fiber_run(vm, args, nargs, rindex, true);
}

static bool fiber_yield(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused(args, nargs, rindex)
	// set rindex slot to NULL in order to falsify the if closure check performed by the VM
	gravity_vm_setslot(vm, GravityValue::from_null(), rindex);
	// get currently executed fiber
	gravity_fiber_t * fiber = gravity_vm_fiber(vm);
	// reset wait time and update last time
	fiber->timewait = 0.0f;
	fiber->lasttime = nanotime();
	// in no caller then this is just a NOP
	if(fiber->caller) {
		gravity_vm_setfiber(vm, fiber->caller);
		// unhook this fiber from the one that called it
		fiber->caller = NULL;
		fiber->trying = false;
		RETURN_FIBER();
	}
	else {
		RETURN_NOVALUE();
	}
}

static bool fiber_yield_time(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused(args, nargs, rindex)
	// set rindex slot to NULL in order to falsify the if closure check performed by the VM
	gravity_vm_setslot(vm, GravityValue::from_null(), rindex);
	// get currently executed fiber
	gravity_fiber_t * fiber = gravity_vm_fiber(vm);
	// if parameter is a float/int set its wait time (otherwise ignore it)
	if(args[1].IsFloat()) {
		fiber->timewait = args[1].GetFloat();
	}
	else if(args[1].IsInt()) {
		fiber->timewait = (gravity_float_t)args[1].n;
	}
	// update last time
	fiber->lasttime = nanotime();

	// in no caller then this is just a NOP
	if(fiber->caller) {
		gravity_vm_setfiber(vm, fiber->caller);
		// unhook this fiber from the one that called it
		fiber->caller = NULL;
		fiber->trying = false;
		RETURN_FIBER();
	}
	else {
		RETURN_NOVALUE();
	}
}

static bool fiber_done(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused(nargs)
	// returns true is the fiber has terminated execution
	gravity_fiber_t * fiber = VALUE_AS_FIBER(args[0]);
	RETURN_VALUE(VALUE_FROM_BOOL(fiber->nframes == 0 || fiber->error), rindex);
}

static bool fiber_status(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused(nargs)
	// status codes:
	// 0    never executed
	// 1    aborted with error
	// 2    terminated
	// 3    running
	// 4    trying

	gravity_fiber_t * fiber = VALUE_AS_FIBER(args[0]);
	if(fiber->error) RETURN_VALUE(GravityValue::from_int(FIBER_ABORTED_WITH_ERROR), rindex);
	if(fiber->nframes == 0) RETURN_VALUE(GravityValue::from_int(FIBER_TERMINATED), rindex);
	RETURN_VALUE(GravityValue::from_int(fiber->status), rindex);
}

static bool fiber_elapsed_time(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused(nargs)
	gravity_fiber_t * fiber = VALUE_AS_FIBER(args[0]);
	RETURN_VALUE(GravityValue::from_float(fiber->elapsedtime), rindex);
}

static bool fiber_abort(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
	GravityValue msg = (nargs > 0) ? args[1] : GravityValue::from_null();
	if(!msg.IsString()) 
		RETURN_ERROR("Fiber.abort expects a string as argument.");
	gravity_string_t * s = static_cast<gravity_string_t *>(msg);
	RETURN_ERROR("%.*s", s->len, s->s);
}

// MARK: - Null Class -

static bool operator_null_add(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused(vm,nargs)
	// NULL + v2 = v2
	RETURN_VALUE(args[1], rindex);
}

static bool operator_null_sub(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
	// NULL - v2 should be computed as -v2 in my opinion (since NULL is interpreted as zero)
	args[0] = GravityValue::from_int(0);
	return operator_int_sub(vm, args, nargs, rindex);
}

static bool operator_null_div(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused(vm,args,nargs)
	// NULL / v2 = 0
	RETURN_VALUE(GravityValue::from_int(0), rindex);
}

static bool operator_null_mul(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused(vm,args,nargs)
	// NULL * v2 = 0
	RETURN_VALUE(GravityValue::from_int(0), rindex);
}

static bool operator_null_rem(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused(vm,args,nargs)
	// NULL % v2 = 0
	RETURN_VALUE(GravityValue::from_int(0), rindex);
}

static bool operator_null_and(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused(vm,args,nargs)
	RETURN_VALUE(VALUE_FROM_BOOL(0), rindex);
}

static bool operator_null_or(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused(vm,nargs)
	GravityValue v2 = args[1];
	INTERNAL_CONVERT_BOOL(v2, true);
	RETURN_VALUE(VALUE_FROM_BOOL(0 || v2.n), rindex);
}

// unary operators
static bool operator_null_neg(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused(vm,args,nargs)
	RETURN_VALUE(VALUE_FROM_BOOL(0), rindex);
}

static bool operator_null_not(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused(vm,args,nargs)
	// !null = true in all the tested programming languages
	RETURN_VALUE(VALUE_FROM_BOOL(1), rindex);
}

#if GRAVITY_NULL_SILENT
static bool operator_null_silent(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused(vm,args,nargs)
	GravityValue key = args[1];
	if(key.IsString()) {
		gravity_object_t * obj = (gravity_object_t *)gravity_class_lookup(GravityEnv.P_ClsNull, key);
		if(obj) RETURN_VALUE(VALUE_FROM_OBJECT(obj), rindex);
	}
	gravity_delegate_t * delegate = gravity_vm_delegate(vm);
	if(delegate->report_null_errors) {
		RETURN_ERROR("Unable to find %s into null object", key.IsString() ? key.GetZString() : "N/A");
	}
	// every operation on NULL returns NULL
	RETURN_VALUE(GravityValue::from_null(), rindex);
}

static bool operator_store_null_silent(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused(vm,args,nargs,rindex)
	gravity_delegate_t * delegate = gravity_vm_delegate(vm);
	if(delegate->report_null_errors) {
		GravityValue key = args[1];
		RETURN_ERROR("Unable to find %s into null object", key.IsString() ? key.GetZString() : "N/A");
	}
	// do not touch any register, a store op on NULL is a NOP operation
	return true;
}

#endif

static bool operator_null_cmp(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused(vm,nargs)
	if(VALUE_ISA_UNDEFINED(args[0])) {
		// undefined case (undefined is equal ONLY to undefined)
		if(VALUE_ISA_UNDEFINED(args[1])) 
			RETURN_VALUE(VALUE_FROM_BOOL(1), rindex);
		RETURN_VALUE(VALUE_FROM_BOOL(0), rindex);
	}
	args[0] = GravityValue::from_int(0);
	return operator_int_cmp(vm, args, nargs, rindex);
}

static bool null_exec(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused(vm, args, nargs)
	RETURN_VALUE(GravityValue::from_null(), rindex);
}

static bool null_iterator(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused(vm, args, nargs)
	RETURN_VALUE(VALUE_FROM_FALSE, rindex);
}

// MARK: - System -

static bool system_nanotime(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused(args,nargs)
	nanotime_t t = nanotime();
	RETURN_VALUE(GravityValue::from_int(t), rindex);
}

static bool system_realprint(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex, bool cr) 
{
    //#pragma unused (rindex)
	for(uint16 i = 1; i < nargs; ++i) {
		GravityValue v = args[i];
		INTERNAL_CONVERT_STRING(v, true);
		gravity_string_t * s = static_cast<gravity_string_t *>(v);
		printf("%.*s", s->len, s->s);
	}
	if(cr) 
		printf("\n");
	RETURN_NOVALUE();
}

static bool system_put(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
	{ return system_realprint(vm, args, nargs, rindex, false); }
static bool system_print(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
	{ return system_realprint(vm, args, nargs, rindex, true); }

static bool system_get(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused (args, nargs)
	GravityValue key = args[1];
	if(!key.IsString()) RETURN_VALUE(GravityValue::from_null(), rindex);
	RETURN_VALUE(gravity_vm_get(vm, key.GetZString()), rindex);
}

static bool system_set(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused (nargs, rindex)
	GravityValue key = args[1];
	GravityValue value = args[2];
	if(!key.IsString()) 
		RETURN_NOVALUE();
	bool result = gravity_vm_set(vm, key.GetZString(), value);
	if(!result) 
		RETURN_ERROR("Unable to apply System setting.");
	RETURN_NOVALUE();
}

static bool system_exit(gravity_vm * vm, GravityValue * args, uint16 nargs, uint32 rindex) 
{
    //#pragma unused (vm, nargs, rindex)
	int n = args[1].IsInt() ? (int)args[1].n : 0;
	exit(n);
	RETURN_NOVALUE();
}

// MARK: - CORE -

gravity_closure_t * FASTCALL computed_property_create(gravity_vm * vm, gravity_function_t * getter_func, gravity_function_t * setter_func) 
{
	gravity_gc_setenabled(vm, false);
	gravity_closure_t * getter_closure = (getter_func) ? gravity_closure_new(vm, getter_func) : NULL;
	gravity_closure_t * setter_closure = (setter_func) ? gravity_closure_new(vm, setter_func) : NULL;
	gravity_function_t * f = gravity_function_new_special(vm, NULL, GRAVITY_COMPUTED_INDEX, getter_closure, setter_closure);
	gravity_gc_setenabled(vm, true);
	return gravity_closure_new(vm, f);
}

void computed_property_free(gravity_class_t * c, const char * name, bool remove_flag) 
{
	STATICVALUE_FROM_STRING(key, name, strlen(name));
	gravity_closure_t * closure = gravity_class_lookup_closure(c, key);
	assert(closure);
	gravity_closure_t * getter = static_cast<gravity_closure_t *>(closure->f->special[0]);
	gravity_closure_t * setter = (closure->f->special[0] != closure->f->special[1]) ? (gravity_closure_t *)closure->f->special[1] : NULL;
	if(getter) {
		gravity_function_t * f = getter->f;
		gravity_closure_free(NULL, getter);
		gravity_function_free(NULL, f);
	}
	if(setter) {
		gravity_function_t * f = setter->f;
		gravity_closure_free(NULL, setter);
		gravity_function_free(NULL, f);
	}
	gravity_function_free(NULL, closure->f);
	gravity_closure_free(NULL, closure);
	if(remove_flag) gravity_hash_remove(c->htable, key);
}

void gravity_core_init() 
{
	// this function must be executed ONCE
	if(core_inited) 
		return;
	core_inited = true;
	//mem_check(false);

	// Creation order here is very important
	// for example in a earlier version the intrinsic classes
	// were created before the Function class
	// so when the isa pointer was set to GravityEnv.P_ClsFunc
	// it resulted in a NULL pointer

	// Object and Class are special classes
	// Object has no superclass (so the lookup loop knows when to finish)
	// Class has Object as its superclass
	// Any other class without an explicit superclass automatically has Object as its super
	// Both Object and Class classes has Class set as metaclass
	// Any other class created with gravity_class_new_pair has "class meta" as its metaclass

	//        CORE CLASS DIAGRAM:
	//
	//        ---->    means class's superclass
	//        ====>    means class's metaclass
	//
	//
	//           +--------------------+    +=========+
	//           |                    |    ||       ||
	//           v                    |    \/       ||
	//        +--------------+     +--------------+   ||
	//        |    Object    | ==> |     Class    |====+
	//        +--------------+     +--------------+
	//             ^                    ^
	//             |                    |
	//        +--------------+     +--------------+
	//        |     Base     | ==> |   Base meta  |
	//        +--------------+     +--------------+
	//             ^                    ^
	//             |                    |
	//        +--------------+     +--------------+
	//        |   Subclass   | ==> |Subclass meta |
	//        +--------------+     +--------------+

	// Create classes first and then bind methods
	// A class without a superclass in a subclass of Object.

	// every class without a super will have OBJECT as its superclass
	GravityEnv.P_ClsObj = gravity_class_new_single(NULL, GRAVITY_CLASS_OBJECT_NAME, 0);
	GravityEnv.P_ClsClass = gravity_class_new_single(NULL, GRAVITY_CLASS_CLASS_NAME, 0);
	gravity_class_setsuper(GravityEnv.P_ClsClass, GravityEnv.P_ClsObj);

	// manually set meta class and isa pointer for classes created without the gravity_class_new_pair
	// when gravity_class_new_single was called GravityEnv.P_ClsClass was NULL so the isa pointer must be reset
	GravityEnv.P_ClsObj->objclass = GravityEnv.P_ClsClass; GravityEnv.P_ClsObj->isa = GravityEnv.P_ClsClass;
	GravityEnv.P_ClsClass->objclass = GravityEnv.P_ClsClass; GravityEnv.P_ClsClass->isa = GravityEnv.P_ClsClass;

	// NULL in gravity_class_new_pair and NEW_FUNCTION macro because I do not want them to be in the GC
	GravityEnv.P_ClsFunc = gravity_class_new_pair(NULL, GRAVITY_CLASS_FUNCTION_NAME, NULL, 0, 0);
	GravityEnv.P_ClsFiber = gravity_class_new_pair(NULL, GRAVITY_CLASS_FIBER_NAME, NULL, 0, 0);
	GravityEnv.P_ClsInstance = gravity_class_new_pair(NULL, GRAVITY_CLASS_INSTANCE_NAME, NULL, 0, 0);
	GravityEnv.P_ClsClosure = gravity_class_new_pair(NULL, GRAVITY_CLASS_CLOSURE_NAME, NULL, 0, 0);
	GravityEnv.P_ClsUpValue = gravity_class_new_pair(NULL, GRAVITY_CLASS_UPVALUE_NAME, NULL, 0, 0);
	GravityEnv.P_ClsModule = NULL;

	// create intrinsic classes (intrinsic datatypes are: Int, Float, Bool, Null, String, List, Map and Range)
	GravityEnv.P_ClsInt = gravity_class_new_pair(NULL, GRAVITY_CLASS_INT_NAME, NULL, 0, 0);
	GravityEnv.P_ClsFloat = gravity_class_new_pair(NULL, GRAVITY_CLASS_FLOAT_NAME, NULL, 0, 0);
	GravityEnv.P_ClsBool = gravity_class_new_pair(NULL, GRAVITY_CLASS_BOOL_NAME, NULL, 0, 0);
	GravityEnv.P_ClsNull = gravity_class_new_pair(NULL, GRAVITY_CLASS_NULL_NAME, NULL, 0, 0);
	GravityEnv.P_ClsString = gravity_class_new_pair(NULL, GRAVITY_CLASS_STRING_NAME, NULL, 0, 0);
	GravityEnv.P_ClsList = gravity_class_new_pair(NULL, GRAVITY_CLASS_LIST_NAME, NULL, 0, 0);
	GravityEnv.P_ClsMap = gravity_class_new_pair(NULL, GRAVITY_CLASS_MAP_NAME, NULL, 0, 0);
	GravityEnv.P_ClsRange = gravity_class_new_pair(NULL, GRAVITY_CLASS_RANGE_NAME, NULL, 0, 0);

	// OBJECT CLASS
	gravity_class_bind(GravityEnv.P_ClsObj, GRAVITY_CLASS_CLASS_NAME, NEW_CLOSURE_VALUE(object_class));
	gravity_class_bind(GravityEnv.P_ClsObj, GRAVITY_OPERATOR_IS_NAME, NEW_CLOSURE_VALUE(object_is));
	gravity_class_bind(GravityEnv.P_ClsObj, GRAVITY_OPERATOR_CMP_NAME, NEW_CLOSURE_VALUE(object_cmp));
	gravity_class_bind(GravityEnv.P_ClsObj, GRAVITY_OPERATOR_EQQ_NAME, NEW_CLOSURE_VALUE(object_eqq));
	gravity_class_bind(GravityEnv.P_ClsObj, GRAVITY_OPERATOR_NEQQ_NAME, NEW_CLOSURE_VALUE(object_neqq));
	gravity_class_bind(GravityEnv.P_ClsObj, GRAVITY_CLASS_INT_NAME, NEW_CLOSURE_VALUE(convert_object_int));
	gravity_class_bind(GravityEnv.P_ClsObj, GRAVITY_CLASS_FLOAT_NAME, NEW_CLOSURE_VALUE(convert_object_float));
	gravity_class_bind(GravityEnv.P_ClsObj, GRAVITY_CLASS_BOOL_NAME, NEW_CLOSURE_VALUE(convert_object_bool));
	gravity_class_bind(GravityEnv.P_ClsObj, GRAVITY_CLASS_STRING_NAME, NEW_CLOSURE_VALUE(convert_object_string));
	gravity_class_bind(GravityEnv.P_ClsObj, GRAVITY_INTERNAL_LOAD_NAME, NEW_CLOSURE_VALUE(object_load));
	gravity_class_bind(GravityEnv.P_ClsObj, GRAVITY_INTERNAL_STORE_NAME, NEW_CLOSURE_VALUE(object_store));
	gravity_class_bind(GravityEnv.P_ClsObj, GRAVITY_INTERNAL_NOTFOUND_NAME, NEW_CLOSURE_VALUE(object_notfound));
	gravity_class_bind(GravityEnv.P_ClsObj, "_size", NEW_CLOSURE_VALUE(object_internal_size));
	gravity_class_bind(GravityEnv.P_ClsObj, GRAVITY_OPERATOR_NOT_NAME, NEW_CLOSURE_VALUE(object_not));
	gravity_class_bind(GravityEnv.P_ClsObj, "bind", NEW_CLOSURE_VALUE(object_bind));
	gravity_class_bind(GravityEnv.P_ClsObj, "unbind", NEW_CLOSURE_VALUE(object_unbind));
	gravity_class_bind(GravityEnv.P_ClsObj, GRAVITY_INTERNAL_EXEC_NAME, NEW_CLOSURE_VALUE(object_exec));
	gravity_class_bind(GravityEnv.P_ClsObj, "clone", NEW_CLOSURE_VALUE(object_clone));

	// INTROSPECTION support added to OBJECT CLASS
	gravity_class_bind(GravityEnv.P_ClsObj, "class", VALUE_FROM_OBJECT(reinterpret_cast<gravity_object_t *>(computed_property_create(NULL, NEW_FUNCTION(object_class), NULL))));
	gravity_class_bind(GravityEnv.P_ClsObj, "meta", VALUE_FROM_OBJECT(reinterpret_cast<gravity_object_t *>(computed_property_create(NULL, NEW_FUNCTION(object_meta), NULL))));
	gravity_class_bind(GravityEnv.P_ClsObj, "respondTo", NEW_CLOSURE_VALUE(object_respond));
	gravity_class_bind(GravityEnv.P_ClsObj, "methods", NEW_CLOSURE_VALUE(object_methods));
	gravity_class_bind(GravityEnv.P_ClsObj, "properties", NEW_CLOSURE_VALUE(object_properties));
	gravity_class_bind(GravityEnv.P_ClsObj, "introspection", NEW_CLOSURE_VALUE(object_introspection));

	// NULL CLASS
	// Meta
	gravity_class_t * null_meta = gravity_class_get_meta(GravityEnv.P_ClsNull);
	gravity_class_bind(null_meta, GRAVITY_INTERNAL_EXEC_NAME, NEW_CLOSURE_VALUE(null_exec));

	// CLASS CLASS
	gravity_class_bind(GravityEnv.P_ClsClass, "name", NEW_CLOSURE_VALUE(class_name));
	gravity_class_bind(GravityEnv.P_ClsClass, GRAVITY_INTERNAL_EXEC_NAME, NEW_CLOSURE_VALUE(class_exec));

	// CLOSURE CLASS
	gravity_class_bind(GravityEnv.P_ClsClosure, "disassemble", NEW_CLOSURE_VALUE(closure_disassemble));
	gravity_class_bind(GravityEnv.P_ClsClosure, "apply", NEW_CLOSURE_VALUE(closure_apply));
	gravity_class_bind(GravityEnv.P_ClsClosure, "bind", NEW_CLOSURE_VALUE(closure_bind));

	// FUNCTION CLASS
	gravity_class_bind(GravityEnv.P_ClsFunc, GRAVITY_INTERNAL_EXEC_NAME, NEW_CLOSURE_VALUE(function_exec));
	gravity_class_bind(GravityEnv.P_ClsFunc, "closure", NEW_CLOSURE_VALUE(function_closure));

	// LIST CLASS
	gravity_closure_t * closure = computed_property_create(NULL, NEW_FUNCTION(list_count), NULL);
	gravity_class_bind(GravityEnv.P_ClsList, "count", VALUE_FROM_OBJECT(reinterpret_cast<gravity_object_t *>(closure)));
	gravity_class_bind(GravityEnv.P_ClsList, ITERATOR_INIT_FUNCTION, NEW_CLOSURE_VALUE(list_iterator));
	gravity_class_bind(GravityEnv.P_ClsList, ITERATOR_NEXT_FUNCTION, NEW_CLOSURE_VALUE(list_iterator_next));
	gravity_class_bind(GravityEnv.P_ClsList, GRAVITY_INTERNAL_LOADAT_NAME, NEW_CLOSURE_VALUE(list_loadat));
	gravity_class_bind(GravityEnv.P_ClsList, GRAVITY_INTERNAL_STOREAT_NAME, NEW_CLOSURE_VALUE(list_storeat));
	gravity_class_bind(GravityEnv.P_ClsList, GRAVITY_INTERNAL_LOOP_NAME, NEW_CLOSURE_VALUE(list_loop));
	gravity_class_bind(GravityEnv.P_ClsList, "join", NEW_CLOSURE_VALUE(list_join));
	gravity_class_bind(GravityEnv.P_ClsList, "push", NEW_CLOSURE_VALUE(list_push));
	gravity_class_bind(GravityEnv.P_ClsList, "pop", NEW_CLOSURE_VALUE(list_pop));
	gravity_class_bind(GravityEnv.P_ClsList, "contains", NEW_CLOSURE_VALUE(list_contains));
	gravity_class_bind(GravityEnv.P_ClsList, "remove", NEW_CLOSURE_VALUE(list_remove));
	gravity_class_bind(GravityEnv.P_ClsList, "indexOf", NEW_CLOSURE_VALUE(list_indexOf));
	gravity_class_bind(GravityEnv.P_ClsList, "reverse", NEW_CLOSURE_VALUE(list_reverse));
	gravity_class_bind(GravityEnv.P_ClsList, "reversed", NEW_CLOSURE_VALUE(list_reversed));
	gravity_class_bind(GravityEnv.P_ClsList, "sort", NEW_CLOSURE_VALUE(list_sort));
	gravity_class_bind(GravityEnv.P_ClsList, "sorted", NEW_CLOSURE_VALUE(list_sorted));
	gravity_class_bind(GravityEnv.P_ClsList, "map", NEW_CLOSURE_VALUE(list_map));
	gravity_class_bind(GravityEnv.P_ClsList, "filter", NEW_CLOSURE_VALUE(list_filter));
	gravity_class_bind(GravityEnv.P_ClsList, "reduce", NEW_CLOSURE_VALUE(list_reduce));
	// Meta
	gravity_class_t * list_meta = gravity_class_get_meta(GravityEnv.P_ClsList);
	gravity_class_bind(list_meta, GRAVITY_INTERNAL_EXEC_NAME, NEW_CLOSURE_VALUE(list_exec));

	// MAP CLASS
	gravity_class_bind(GravityEnv.P_ClsMap, "keys", NEW_CLOSURE_VALUE(map_keys));
	gravity_class_bind(GravityEnv.P_ClsMap, "remove", NEW_CLOSURE_VALUE(map_remove));
	closure = computed_property_create(NULL, NEW_FUNCTION(map_count), NULL);
	gravity_class_bind(GravityEnv.P_ClsMap, "count", VALUE_FROM_OBJECT(reinterpret_cast<gravity_object_t *>(closure)));
	gravity_class_bind(GravityEnv.P_ClsMap, GRAVITY_INTERNAL_LOOP_NAME, NEW_CLOSURE_VALUE(map_loop));
	gravity_class_bind(GravityEnv.P_ClsMap, GRAVITY_INTERNAL_LOADAT_NAME, NEW_CLOSURE_VALUE(map_loadat));
	gravity_class_bind(GravityEnv.P_ClsMap, GRAVITY_INTERNAL_STOREAT_NAME, NEW_CLOSURE_VALUE(map_storeat));
	gravity_class_bind(GravityEnv.P_ClsMap, "hasKey", NEW_CLOSURE_VALUE(map_haskey));
	gravity_class_bind(GravityEnv.P_ClsMap, ITERATOR_INIT_FUNCTION, NEW_CLOSURE_VALUE(map_iterator));
    #if GRAVITY_MAP_DOTSUGAR
	gravity_class_bind(GravityEnv.P_ClsMap, GRAVITY_INTERNAL_LOAD_NAME, NEW_CLOSURE_VALUE(map_load));
	gravity_class_bind(GravityEnv.P_ClsMap, GRAVITY_INTERNAL_STORE_NAME, NEW_CLOSURE_VALUE(map_storeat));
    #endif

	// RANGE CLASS
	closure = computed_property_create(NULL, NEW_FUNCTION(range_count), NULL);
	gravity_class_bind(GravityEnv.P_ClsRange, "count", VALUE_FROM_OBJECT(reinterpret_cast<gravity_object_t *>(closure)));
	closure = computed_property_create(NULL, NEW_FUNCTION(range_from), NULL);
	gravity_class_bind(GravityEnv.P_ClsRange, "from", VALUE_FROM_OBJECT(reinterpret_cast<gravity_object_t *>(closure)));
	closure = computed_property_create(NULL, NEW_FUNCTION(range_to), NULL);
	gravity_class_bind(GravityEnv.P_ClsRange, "to", VALUE_FROM_OBJECT(reinterpret_cast<gravity_object_t *>(closure)));
	gravity_class_bind(GravityEnv.P_ClsRange, ITERATOR_INIT_FUNCTION, NEW_CLOSURE_VALUE(range_iterator));
	gravity_class_bind(GravityEnv.P_ClsRange, ITERATOR_NEXT_FUNCTION, NEW_CLOSURE_VALUE(range_iterator_next));
	gravity_class_bind(GravityEnv.P_ClsRange, "contains", NEW_CLOSURE_VALUE(range_contains));
	gravity_class_bind(GravityEnv.P_ClsRange, GRAVITY_INTERNAL_LOOP_NAME, NEW_CLOSURE_VALUE(range_loop));
	// Meta
	gravity_class_t * range_meta = gravity_class_get_meta(GravityEnv.P_ClsRange);
	gravity_class_bind(range_meta, GRAVITY_INTERNAL_EXEC_NAME, NEW_CLOSURE_VALUE(range_exec));

	// INT CLASS
	gravity_class_bind(GravityEnv.P_ClsInt, GRAVITY_OPERATOR_ADD_NAME, NEW_CLOSURE_VALUE(operator_int_add));
	gravity_class_bind(GravityEnv.P_ClsInt, GRAVITY_OPERATOR_SUB_NAME, NEW_CLOSURE_VALUE(operator_int_sub));
	gravity_class_bind(GravityEnv.P_ClsInt, GRAVITY_OPERATOR_DIV_NAME, NEW_CLOSURE_VALUE(operator_int_div));
	gravity_class_bind(GravityEnv.P_ClsInt, GRAVITY_OPERATOR_MUL_NAME, NEW_CLOSURE_VALUE(operator_int_mul));
	gravity_class_bind(GravityEnv.P_ClsInt, GRAVITY_OPERATOR_REM_NAME, NEW_CLOSURE_VALUE(operator_int_rem));
	gravity_class_bind(GravityEnv.P_ClsInt, GRAVITY_OPERATOR_AND_NAME, NEW_CLOSURE_VALUE(operator_int_and));
	gravity_class_bind(GravityEnv.P_ClsInt, GRAVITY_OPERATOR_OR_NAME,  NEW_CLOSURE_VALUE(operator_int_or));
	gravity_class_bind(GravityEnv.P_ClsInt, GRAVITY_OPERATOR_CMP_NAME, NEW_CLOSURE_VALUE(operator_int_cmp));
//    gravity_class_bind(GravityEnv.P_ClsInt, GRAVITY_OPERATOR_BITOR_NAME, NEW_CLOSURE_VALUE(operator_int_bitor));
//    gravity_class_bind(GravityEnv.P_ClsInt, GRAVITY_OPERATOR_BITAND_NAME, NEW_CLOSURE_VALUE(operator_int_bitand));
//    gravity_class_bind(GravityEnv.P_ClsInt, GRAVITY_OPERATOR_BITXOR_NAME, NEW_CLOSURE_VALUE(operator_int_bitxor));
//    gravity_class_bind(GravityEnv.P_ClsInt, GRAVITY_OPERATOR_BITLS_NAME, NEW_CLOSURE_VALUE(operator_int_bitls));
//    gravity_class_bind(GravityEnv.P_ClsInt, GRAVITY_OPERATOR_BITRS_NAME, NEW_CLOSURE_VALUE(operator_int_bitrs));
	gravity_class_bind(GravityEnv.P_ClsInt, GRAVITY_OPERATOR_NEG_NAME, NEW_CLOSURE_VALUE(operator_int_neg));
	gravity_class_bind(GravityEnv.P_ClsInt, GRAVITY_OPERATOR_NOT_NAME, NEW_CLOSURE_VALUE(operator_int_not));
	gravity_class_bind(GravityEnv.P_ClsInt, GRAVITY_INTERNAL_LOOP_NAME, NEW_CLOSURE_VALUE(int_loop));
	closure = computed_property_create(NULL, NEW_FUNCTION(int_radians), NULL);
	gravity_class_bind(GravityEnv.P_ClsInt, "radians", VALUE_FROM_OBJECT(reinterpret_cast<gravity_object_t *>(closure)));
	closure = computed_property_create(NULL, NEW_FUNCTION(int_degrees), NULL);
	gravity_class_bind(GravityEnv.P_ClsInt, "degrees", VALUE_FROM_OBJECT(reinterpret_cast<gravity_object_t *>(closure)));
	// Meta
	gravity_class_t * int_meta = gravity_class_get_meta(GravityEnv.P_ClsInt);
	gravity_class_bind(int_meta, "random", NEW_CLOSURE_VALUE(int_random));
	gravity_class_bind(int_meta, GRAVITY_INTERNAL_EXEC_NAME, NEW_CLOSURE_VALUE(int_exec));
	gravity_class_bind(int_meta, "min", VALUE_FROM_OBJECT(reinterpret_cast<gravity_object_t *>(computed_property_create(NULL, NEW_FUNCTION(int_min), NULL))));
	gravity_class_bind(int_meta, "max", VALUE_FROM_OBJECT(reinterpret_cast<gravity_object_t *>(computed_property_create(NULL, NEW_FUNCTION(int_max), NULL))));

	// FLOAT CLASS
	gravity_class_bind(GravityEnv.P_ClsFloat, GRAVITY_OPERATOR_ADD_NAME, NEW_CLOSURE_VALUE(operator_float_add));
	gravity_class_bind(GravityEnv.P_ClsFloat, GRAVITY_OPERATOR_SUB_NAME, NEW_CLOSURE_VALUE(operator_float_sub));
	gravity_class_bind(GravityEnv.P_ClsFloat, GRAVITY_OPERATOR_DIV_NAME, NEW_CLOSURE_VALUE(operator_float_div));
	gravity_class_bind(GravityEnv.P_ClsFloat, GRAVITY_OPERATOR_MUL_NAME, NEW_CLOSURE_VALUE(operator_float_mul));
	gravity_class_bind(GravityEnv.P_ClsFloat, GRAVITY_OPERATOR_REM_NAME, NEW_CLOSURE_VALUE(operator_float_rem));
	gravity_class_bind(GravityEnv.P_ClsFloat, GRAVITY_OPERATOR_AND_NAME, NEW_CLOSURE_VALUE(operator_float_and));
	gravity_class_bind(GravityEnv.P_ClsFloat, GRAVITY_OPERATOR_OR_NAME,  NEW_CLOSURE_VALUE(operator_float_or));
	gravity_class_bind(GravityEnv.P_ClsFloat, GRAVITY_OPERATOR_CMP_NAME, NEW_CLOSURE_VALUE(operator_float_cmp));
	gravity_class_bind(GravityEnv.P_ClsFloat, GRAVITY_OPERATOR_NEG_NAME, NEW_CLOSURE_VALUE(operator_float_neg));
	gravity_class_bind(GravityEnv.P_ClsFloat, GRAVITY_OPERATOR_NOT_NAME, NEW_CLOSURE_VALUE(operator_float_not));
	gravity_class_bind(GravityEnv.P_ClsFloat, "round", NEW_CLOSURE_VALUE(function_float_round));
	gravity_class_bind(GravityEnv.P_ClsFloat, "isClose", NEW_CLOSURE_VALUE(float_isclose));
	gravity_class_bind(GravityEnv.P_ClsFloat, "floor", NEW_CLOSURE_VALUE(function_float_floor));
	gravity_class_bind(GravityEnv.P_ClsFloat, "ceil", NEW_CLOSURE_VALUE(function_float_ceil));
	closure = computed_property_create(NULL, NEW_FUNCTION(float_radians), NULL);
	gravity_class_bind(GravityEnv.P_ClsFloat, "radians", VALUE_FROM_OBJECT(reinterpret_cast<gravity_object_t *>(closure)));
	closure = computed_property_create(NULL, NEW_FUNCTION(float_degrees), NULL);
	gravity_class_bind(GravityEnv.P_ClsFloat, "degrees", VALUE_FROM_OBJECT(reinterpret_cast<gravity_object_t *>(closure)));
	// Meta
	gravity_class_t * float_meta = gravity_class_get_meta(GravityEnv.P_ClsFloat);
	gravity_class_bind(float_meta, GRAVITY_INTERNAL_EXEC_NAME, NEW_CLOSURE_VALUE(float_exec));
	gravity_class_bind(float_meta, "min", VALUE_FROM_OBJECT(reinterpret_cast<gravity_object_t *>(computed_property_create(NULL, NEW_FUNCTION(float_min), NULL))));
	gravity_class_bind(float_meta, "max", VALUE_FROM_OBJECT(reinterpret_cast<gravity_object_t *>(computed_property_create(NULL, NEW_FUNCTION(float_max), NULL))));

	// BOOL CLASS
	gravity_class_bind(GravityEnv.P_ClsBool, GRAVITY_OPERATOR_ADD_NAME, NEW_CLOSURE_VALUE(operator_bool_add));
	gravity_class_bind(GravityEnv.P_ClsBool, GRAVITY_OPERATOR_SUB_NAME, NEW_CLOSURE_VALUE(operator_bool_sub));
	gravity_class_bind(GravityEnv.P_ClsBool, GRAVITY_OPERATOR_DIV_NAME, NEW_CLOSURE_VALUE(operator_bool_div));
	gravity_class_bind(GravityEnv.P_ClsBool, GRAVITY_OPERATOR_MUL_NAME, NEW_CLOSURE_VALUE(operator_bool_mul));
	gravity_class_bind(GravityEnv.P_ClsBool, GRAVITY_OPERATOR_REM_NAME, NEW_CLOSURE_VALUE(operator_bool_rem));
	gravity_class_bind(GravityEnv.P_ClsBool, GRAVITY_OPERATOR_AND_NAME, NEW_CLOSURE_VALUE(operator_bool_and));
	gravity_class_bind(GravityEnv.P_ClsBool, GRAVITY_OPERATOR_OR_NAME,  NEW_CLOSURE_VALUE(operator_bool_or));
	gravity_class_bind(GravityEnv.P_ClsBool, GRAVITY_OPERATOR_BOR_NAME, NEW_CLOSURE_VALUE(operator_bool_bitor));
	gravity_class_bind(GravityEnv.P_ClsBool, GRAVITY_OPERATOR_BAND_NAME, NEW_CLOSURE_VALUE(operator_bool_bitand));
	gravity_class_bind(GravityEnv.P_ClsBool, GRAVITY_OPERATOR_BXOR_NAME, NEW_CLOSURE_VALUE(operator_bool_bitxor));
	gravity_class_bind(GravityEnv.P_ClsBool, GRAVITY_OPERATOR_CMP_NAME, NEW_CLOSURE_VALUE(operator_bool_cmp));
	gravity_class_bind(GravityEnv.P_ClsBool, GRAVITY_OPERATOR_NEG_NAME, NEW_CLOSURE_VALUE(operator_bool_neg));
	gravity_class_bind(GravityEnv.P_ClsBool, GRAVITY_OPERATOR_NOT_NAME, NEW_CLOSURE_VALUE(operator_bool_not));
	// Meta
	gravity_class_t * bool_meta = gravity_class_get_meta(GravityEnv.P_ClsBool);
	gravity_class_bind(bool_meta, GRAVITY_INTERNAL_EXEC_NAME, NEW_CLOSURE_VALUE(bool_exec));

	// STRING CLASS
	gravity_class_bind(GravityEnv.P_ClsString, GRAVITY_OPERATOR_ADD_NAME, NEW_CLOSURE_VALUE(operator_string_add));
	gravity_class_bind(GravityEnv.P_ClsString, GRAVITY_OPERATOR_SUB_NAME, NEW_CLOSURE_VALUE(operator_string_sub));
	gravity_class_bind(GravityEnv.P_ClsString, GRAVITY_OPERATOR_AND_NAME, NEW_CLOSURE_VALUE(operator_string_and));
	gravity_class_bind(GravityEnv.P_ClsString, GRAVITY_OPERATOR_OR_NAME,  NEW_CLOSURE_VALUE(operator_string_or));
	gravity_class_bind(GravityEnv.P_ClsString, GRAVITY_OPERATOR_CMP_NAME, NEW_CLOSURE_VALUE(operator_string_cmp));
	gravity_class_bind(GravityEnv.P_ClsString, GRAVITY_OPERATOR_NEG_NAME, NEW_CLOSURE_VALUE(operator_string_neg));
	gravity_class_bind(GravityEnv.P_ClsString, GRAVITY_INTERNAL_LOADAT_NAME, NEW_CLOSURE_VALUE(string_loadat));
	gravity_class_bind(GravityEnv.P_ClsString, GRAVITY_INTERNAL_STOREAT_NAME, NEW_CLOSURE_VALUE(string_storeat));
	closure = computed_property_create(NULL, NEW_FUNCTION(string_length), NULL);
	gravity_class_bind(GravityEnv.P_ClsString, "length", VALUE_FROM_OBJECT(reinterpret_cast<gravity_object_t *>(closure)));
	closure = computed_property_create(NULL, NEW_FUNCTION(string_bytes), NULL);
	gravity_class_bind(GravityEnv.P_ClsString, "bytes", VALUE_FROM_OBJECT(reinterpret_cast<gravity_object_t *>(closure)));
	gravity_class_bind(GravityEnv.P_ClsString, "index", NEW_CLOSURE_VALUE(string_index));
	gravity_class_bind(GravityEnv.P_ClsString, "contains", NEW_CLOSURE_VALUE(string_contains));
	gravity_class_bind(GravityEnv.P_ClsString, "replace", NEW_CLOSURE_VALUE(string_find_replace));
	gravity_class_bind(GravityEnv.P_ClsString, "count", NEW_CLOSURE_VALUE(string_count));
	gravity_class_bind(GravityEnv.P_ClsString, "repeat", NEW_CLOSURE_VALUE(string_repeat));
	gravity_class_bind(GravityEnv.P_ClsString, "upper", NEW_CLOSURE_VALUE(string_upper));
	gravity_class_bind(GravityEnv.P_ClsString, "lower", NEW_CLOSURE_VALUE(string_lower));
	gravity_class_bind(GravityEnv.P_ClsString, "split", NEW_CLOSURE_VALUE(string_split));
	gravity_class_bind(GravityEnv.P_ClsString, "loop", NEW_CLOSURE_VALUE(string_loop));
	gravity_class_bind(GravityEnv.P_ClsString, ITERATOR_INIT_FUNCTION, NEW_CLOSURE_VALUE(string_iterator));
	gravity_class_bind(GravityEnv.P_ClsString, ITERATOR_NEXT_FUNCTION, NEW_CLOSURE_VALUE(string_iterator_next));
	gravity_class_bind(GravityEnv.P_ClsString, "number", NEW_CLOSURE_VALUE(string_number));
	gravity_class_bind(GravityEnv.P_ClsString, "trim", NEW_CLOSURE_VALUE(string_trim));
	gravity_class_bind(GravityEnv.P_ClsString, "raw", NEW_CLOSURE_VALUE(string_raw));
	gravity_class_bind(GravityEnv.P_ClsString, GRAVITY_TOCLASS_NAME, NEW_CLOSURE_VALUE(string_toclass));

	// Meta
	gravity_class_t * string_meta = gravity_class_get_meta(GravityEnv.P_ClsString);
	gravity_class_bind(string_meta, GRAVITY_INTERNAL_EXEC_NAME, NEW_CLOSURE_VALUE(string_exec));

	// FIBER CLASS
	gravity_class_t * fiber_meta = gravity_class_get_meta(GravityEnv.P_ClsFiber);
	gravity_class_bind(fiber_meta, "create", NEW_CLOSURE_VALUE(fiber_create));
	gravity_class_bind(GravityEnv.P_ClsFiber, "call", NEW_CLOSURE_VALUE(fiber_exec));
	gravity_class_bind(GravityEnv.P_ClsFiber, "try", NEW_CLOSURE_VALUE(fiber_try));
	gravity_class_bind(fiber_meta, "yield", NEW_CLOSURE_VALUE(fiber_yield));
	gravity_class_bind(fiber_meta, "yieldWaitTime", NEW_CLOSURE_VALUE(fiber_yield_time));
	gravity_class_bind(GravityEnv.P_ClsFiber, "status", NEW_CLOSURE_VALUE(fiber_status));
	gravity_class_bind(GravityEnv.P_ClsFiber, "isDone", NEW_CLOSURE_VALUE(fiber_done));
	gravity_class_bind(GravityEnv.P_ClsFiber, "elapsedTime", NEW_CLOSURE_VALUE(fiber_elapsed_time));
	gravity_class_bind(fiber_meta, "abort", NEW_CLOSURE_VALUE(fiber_abort));

	// BASIC OPERATIONS added also to NULL CLASS (and UNDEFINED since they points to the same class)
	// this is required because every variable is initialized by default to NULL
	gravity_class_bind(GravityEnv.P_ClsNull, GRAVITY_OPERATOR_ADD_NAME, NEW_CLOSURE_VALUE(operator_null_add));
	gravity_class_bind(GravityEnv.P_ClsNull, GRAVITY_OPERATOR_SUB_NAME, NEW_CLOSURE_VALUE(operator_null_sub));
	gravity_class_bind(GravityEnv.P_ClsNull, GRAVITY_OPERATOR_DIV_NAME, NEW_CLOSURE_VALUE(operator_null_div));
	gravity_class_bind(GravityEnv.P_ClsNull, GRAVITY_OPERATOR_MUL_NAME, NEW_CLOSURE_VALUE(operator_null_mul));
	gravity_class_bind(GravityEnv.P_ClsNull, GRAVITY_OPERATOR_REM_NAME, NEW_CLOSURE_VALUE(operator_null_rem));
	gravity_class_bind(GravityEnv.P_ClsNull, GRAVITY_OPERATOR_AND_NAME, NEW_CLOSURE_VALUE(operator_null_and));
	gravity_class_bind(GravityEnv.P_ClsNull, GRAVITY_OPERATOR_OR_NAME,  NEW_CLOSURE_VALUE(operator_null_or));
	gravity_class_bind(GravityEnv.P_ClsNull, GRAVITY_OPERATOR_CMP_NAME, NEW_CLOSURE_VALUE(operator_null_cmp));
	gravity_class_bind(GravityEnv.P_ClsNull, GRAVITY_OPERATOR_NEG_NAME, NEW_CLOSURE_VALUE(operator_null_neg));
	gravity_class_bind(GravityEnv.P_ClsNull, GRAVITY_OPERATOR_NOT_NAME, NEW_CLOSURE_VALUE(operator_null_not));
    #if GRAVITY_NULL_SILENT
	gravity_class_bind(GravityEnv.P_ClsNull, GRAVITY_INTERNAL_EXEC_NAME, NEW_CLOSURE_VALUE(operator_null_silent));
	gravity_class_bind(GravityEnv.P_ClsNull, GRAVITY_INTERNAL_LOAD_NAME, NEW_CLOSURE_VALUE(operator_null_silent));
	gravity_class_bind(GravityEnv.P_ClsNull, GRAVITY_INTERNAL_STORE_NAME, NEW_CLOSURE_VALUE(operator_store_null_silent));
	gravity_class_bind(GravityEnv.P_ClsNull, GRAVITY_INTERNAL_NOTFOUND_NAME, NEW_CLOSURE_VALUE(operator_null_silent));
    #endif
	gravity_class_bind(GravityEnv.P_ClsNull, ITERATOR_INIT_FUNCTION, NEW_CLOSURE_VALUE(null_iterator));

	// SYSTEM class
	GravityEnv.P_ClsSystem = gravity_class_new_pair(NULL, GRAVITY_CLASS_SYSTEM_NAME, NULL, 0, 0);
	gravity_class_t * system_meta = gravity_class_get_meta(GravityEnv.P_ClsSystem);
	gravity_class_bind(system_meta, GRAVITY_SYSTEM_NANOTIME_NAME, NEW_CLOSURE_VALUE(system_nanotime));
	gravity_class_bind(system_meta, GRAVITY_SYSTEM_PRINT_NAME, NEW_CLOSURE_VALUE(system_print));
	gravity_class_bind(system_meta, GRAVITY_SYSTEM_PUT_NAME, NEW_CLOSURE_VALUE(system_put));
	gravity_class_bind(system_meta, "exit", NEW_CLOSURE_VALUE(system_exit));

	closure = computed_property_create(NULL, NEW_FUNCTION(system_get), NEW_FUNCTION(system_set));
	GravityValue value = VALUE_FROM_OBJECT(reinterpret_cast<gravity_object_t *>(closure));
	gravity_class_bind(system_meta, GRAVITY_VM_GCENABLED, value);
	gravity_class_bind(system_meta, GRAVITY_VM_GCMINTHRESHOLD, value);
	gravity_class_bind(system_meta, GRAVITY_VM_GCTHRESHOLD, value);
	gravity_class_bind(system_meta, GRAVITY_VM_GCRATIO, value);
	gravity_class_bind(system_meta, GRAVITY_VM_MAXCALLS, value);
	gravity_class_bind(system_meta, GRAVITY_VM_MAXBLOCK, value);
	gravity_class_bind(system_meta, GRAVITY_VM_MAXRECURSION, value);

	// INIT META
	SETMETA_INITED(GravityEnv.P_ClsInt);
	SETMETA_INITED(GravityEnv.P_ClsFloat);
	SETMETA_INITED(GravityEnv.P_ClsBool);
	SETMETA_INITED(GravityEnv.P_ClsNull);
	SETMETA_INITED(GravityEnv.P_ClsString);
	SETMETA_INITED(GravityEnv.P_ClsObj);
	SETMETA_INITED(GravityEnv.P_ClsFunc);
	SETMETA_INITED(GravityEnv.P_ClsClosure);
	SETMETA_INITED(GravityEnv.P_ClsFiber);
	SETMETA_INITED(GravityEnv.P_ClsClass);
	SETMETA_INITED(GravityEnv.P_ClsInstance);
	SETMETA_INITED(GravityEnv.P_ClsList);
	SETMETA_INITED(GravityEnv.P_ClsMap);
	SETMETA_INITED(GravityEnv.P_ClsRange);
	SETMETA_INITED(GravityEnv.P_ClsUpValue);
	SETMETA_INITED(GravityEnv.P_ClsSystem);
	//SETMETA_INITED(GravityEnv.P_ClsModule);

	//mem_check(true);
}

void gravity_core_free() 
{
	// free optionals first
	gravity_opt_free();
	if(!core_inited) 
		return;
	// check if others VM are still running
	if(--refcount) 
		return;
	// this function should never be called
	// it is just called when we need to internally check for memory leaks
	// computed properties are not registered inside VM gc so they need to be manually freed here
	//mem_check(false);
	computed_property_free(GravityEnv.P_ClsList, "count", true);
	computed_property_free(GravityEnv.P_ClsMap, "count", true);
	computed_property_free(GravityEnv.P_ClsRange, "count", true);
	computed_property_free(GravityEnv.P_ClsString, "length", true);
	computed_property_free(GravityEnv.P_ClsInt, "radians", true);
	computed_property_free(GravityEnv.P_ClsInt, "degrees", true);
	computed_property_free(GravityEnv.P_ClsFloat, "radians", true);
	computed_property_free(GravityEnv.P_ClsFloat, "degrees", true);
	gravity_class_t * system_meta = gravity_class_get_meta(GravityEnv.P_ClsSystem);
	computed_property_free(system_meta, GRAVITY_VM_GCENABLED, true);

	gravity_class_free_core(NULL, gravity_class_get_meta(GravityEnv.P_ClsInt));
	gravity_class_free_core(NULL, GravityEnv.P_ClsInt);
	gravity_class_free_core(NULL, gravity_class_get_meta(GravityEnv.P_ClsFloat));
	gravity_class_free_core(NULL, GravityEnv.P_ClsFloat);
	gravity_class_free_core(NULL, gravity_class_get_meta(GravityEnv.P_ClsBool));
	gravity_class_free_core(NULL, GravityEnv.P_ClsBool);
	gravity_class_free_core(NULL, gravity_class_get_meta(GravityEnv.P_ClsString));
	gravity_class_free_core(NULL, GravityEnv.P_ClsString);
	gravity_class_free_core(NULL, gravity_class_get_meta(GravityEnv.P_ClsNull));
	gravity_class_free_core(NULL, GravityEnv.P_ClsNull);
	gravity_class_free_core(NULL, gravity_class_get_meta(GravityEnv.P_ClsFunc));
	gravity_class_free_core(NULL, GravityEnv.P_ClsFunc);
	gravity_class_free_core(NULL, gravity_class_get_meta(GravityEnv.P_ClsClosure));
	gravity_class_free_core(NULL, GravityEnv.P_ClsClosure);
	gravity_class_free_core(NULL, gravity_class_get_meta(GravityEnv.P_ClsFiber));
	gravity_class_free_core(NULL, GravityEnv.P_ClsFiber);
	gravity_class_free_core(NULL, gravity_class_get_meta(GravityEnv.P_ClsInstance));
	gravity_class_free_core(NULL, GravityEnv.P_ClsInstance);
	gravity_class_free_core(NULL, gravity_class_get_meta(GravityEnv.P_ClsList));
	gravity_class_free_core(NULL, GravityEnv.P_ClsList);
	gravity_class_free_core(NULL, gravity_class_get_meta(GravityEnv.P_ClsMap));
	gravity_class_free_core(NULL, GravityEnv.P_ClsMap);
	gravity_class_free_core(NULL, gravity_class_get_meta(GravityEnv.P_ClsRange));
	gravity_class_free_core(NULL, GravityEnv.P_ClsRange);
	gravity_class_free_core(NULL, gravity_class_get_meta(GravityEnv.P_ClsUpValue));
	gravity_class_free_core(NULL, GravityEnv.P_ClsUpValue);

	// before freeing the meta class we need to remove entries with duplicated functions
	{STATICVALUE_FROM_STRING(key, GRAVITY_VM_GCMINTHRESHOLD, strlen(GRAVITY_VM_GCMINTHRESHOLD)); gravity_hash_remove(system_meta->htable, key);}
	{STATICVALUE_FROM_STRING(key, GRAVITY_VM_GCTHRESHOLD, strlen(GRAVITY_VM_GCTHRESHOLD)); gravity_hash_remove(system_meta->htable, key);}
	{STATICVALUE_FROM_STRING(key, GRAVITY_VM_GCRATIO, strlen(GRAVITY_VM_GCRATIO)); gravity_hash_remove(system_meta->htable, key);}
	{STATICVALUE_FROM_STRING(key, GRAVITY_VM_MAXCALLS, strlen(GRAVITY_VM_MAXCALLS)); gravity_hash_remove(system_meta->htable, key);}
	{STATICVALUE_FROM_STRING(key, GRAVITY_VM_MAXBLOCK, strlen(GRAVITY_VM_MAXBLOCK)); gravity_hash_remove(system_meta->htable, key);}
	{STATICVALUE_FROM_STRING(key, GRAVITY_VM_MAXRECURSION, strlen(GRAVITY_VM_MAXRECURSION)); gravity_hash_remove(system_meta->htable, key);}
		gravity_class_free_core(NULL, system_meta); gravity_class_free_core(NULL, GravityEnv.P_ClsSystem);

	// object must be the last class to be freed
	gravity_class_free_core(NULL, GravityEnv.P_ClsClass);
	gravity_class_free_core(NULL, GravityEnv.P_ClsObj);
	//mem_check(true);

	GravityEnv.P_ClsInt = NULL;
	GravityEnv.P_ClsFloat = NULL;
	GravityEnv.P_ClsBool = NULL;
	GravityEnv.P_ClsString = NULL;
	GravityEnv.P_ClsObj = NULL;
	GravityEnv.P_ClsNull = NULL;
	GravityEnv.P_ClsFunc = NULL;
	GravityEnv.P_ClsClosure = NULL;
	GravityEnv.P_ClsFiber = NULL;
	GravityEnv.P_ClsClass = NULL;
	GravityEnv.P_ClsInstance = NULL;
	GravityEnv.P_ClsList = NULL;
	GravityEnv.P_ClsMap = NULL;
	GravityEnv.P_ClsRange = NULL;
	GravityEnv.P_ClsUpValue = NULL;
	GravityEnv.P_ClsSystem = NULL;
	GravityEnv.P_ClsModule = NULL;
	core_inited = false;
}

const char ** gravity_core_identifiers() 
{
	static const char * list[] = {GRAVITY_CLASS_OBJECT_NAME, GRAVITY_CLASS_CLASS_NAME, GRAVITY_CLASS_BOOL_NAME, GRAVITY_CLASS_NULL_NAME,
		GRAVITY_CLASS_INT_NAME, GRAVITY_CLASS_FLOAT_NAME, GRAVITY_CLASS_FUNCTION_NAME,
		GRAVITY_CLASS_FIBER_NAME, GRAVITY_CLASS_STRING_NAME,
		GRAVITY_CLASS_INSTANCE_NAME, GRAVITY_CLASS_LIST_NAME, GRAVITY_CLASS_MAP_NAME,
		GRAVITY_CLASS_RANGE_NAME, GRAVITY_CLASS_SYSTEM_NAME,
		GRAVITY_CLASS_CLOSURE_NAME, GRAVITY_CLASS_UPVALUE_NAME, NULL};
	return list;
}

void gravity_core_register(gravity_vm * vm) 
{
	gravity_core_init();
	gravity_opt_register(vm);
	++refcount;
	if(vm) {
		// register core classes inside VM
		if(!gravity_vm_ismini(vm)) {
			gravity_vm_setvalue(vm, GRAVITY_CLASS_OBJECT_NAME, VALUE_FROM_OBJECT(GravityEnv.P_ClsObj));
			gravity_vm_setvalue(vm, GRAVITY_CLASS_CLASS_NAME, VALUE_FROM_OBJECT(GravityEnv.P_ClsClass));
			gravity_vm_setvalue(vm, GRAVITY_CLASS_BOOL_NAME, VALUE_FROM_OBJECT(GravityEnv.P_ClsBool));
			gravity_vm_setvalue(vm, GRAVITY_CLASS_NULL_NAME, VALUE_FROM_OBJECT(GravityEnv.P_ClsNull));
			gravity_vm_setvalue(vm, GRAVITY_CLASS_INT_NAME, VALUE_FROM_OBJECT(GravityEnv.P_ClsInt));
			gravity_vm_setvalue(vm, GRAVITY_CLASS_FLOAT_NAME, VALUE_FROM_OBJECT(GravityEnv.P_ClsFloat));
			gravity_vm_setvalue(vm, GRAVITY_CLASS_FUNCTION_NAME, VALUE_FROM_OBJECT(GravityEnv.P_ClsFunc));
			gravity_vm_setvalue(vm, GRAVITY_CLASS_CLOSURE_NAME, VALUE_FROM_OBJECT(GravityEnv.P_ClsClosure));
			gravity_vm_setvalue(vm, GRAVITY_CLASS_FIBER_NAME, VALUE_FROM_OBJECT(GravityEnv.P_ClsFiber));
			gravity_vm_setvalue(vm, GRAVITY_CLASS_STRING_NAME, VALUE_FROM_OBJECT(GravityEnv.P_ClsString));
			gravity_vm_setvalue(vm, GRAVITY_CLASS_INSTANCE_NAME, VALUE_FROM_OBJECT(GravityEnv.P_ClsInstance));
			gravity_vm_setvalue(vm, GRAVITY_CLASS_LIST_NAME, VALUE_FROM_OBJECT(GravityEnv.P_ClsList));
			gravity_vm_setvalue(vm, GRAVITY_CLASS_MAP_NAME, VALUE_FROM_OBJECT(GravityEnv.P_ClsMap));
			gravity_vm_setvalue(vm, GRAVITY_CLASS_RANGE_NAME, VALUE_FROM_OBJECT(GravityEnv.P_ClsRange));
			gravity_vm_setvalue(vm, GRAVITY_CLASS_UPVALUE_NAME, VALUE_FROM_OBJECT(GravityEnv.P_ClsUpValue));
			gravity_vm_setvalue(vm, GRAVITY_CLASS_SYSTEM_NAME, VALUE_FROM_OBJECT(GravityEnv.P_ClsSystem));
		}
	}
}

gravity_class_t * gravity_core_class_from_name(const char * name) 
{
	if(name) {
		if(sstreq(name, GRAVITY_CLASS_OBJECT_NAME)) return GravityEnv.P_ClsObj;
		if(sstreq(name, GRAVITY_CLASS_CLASS_NAME)) return GravityEnv.P_ClsClass;
		if(sstreq(name, GRAVITY_CLASS_BOOL_NAME)) return GravityEnv.P_ClsBool;
		if(sstreq(name, GRAVITY_CLASS_NULL_NAME)) return GravityEnv.P_ClsNull;
		if(sstreq(name, GRAVITY_CLASS_INT_NAME)) return GravityEnv.P_ClsInt;
		if(sstreq(name, GRAVITY_CLASS_FLOAT_NAME)) return GravityEnv.P_ClsFloat;
		if(sstreq(name, GRAVITY_CLASS_FUNCTION_NAME)) return GravityEnv.P_ClsFunc;
		if(sstreq(name, GRAVITY_CLASS_CLOSURE_NAME)) return GravityEnv.P_ClsClosure;
		if(sstreq(name, GRAVITY_CLASS_FIBER_NAME)) return GravityEnv.P_ClsFiber;
		if(sstreq(name, GRAVITY_CLASS_STRING_NAME)) return GravityEnv.P_ClsString;
		if(sstreq(name, GRAVITY_CLASS_INSTANCE_NAME)) return GravityEnv.P_ClsInstance;
		if(sstreq(name, GRAVITY_CLASS_LIST_NAME)) return GravityEnv.P_ClsList;
		if(sstreq(name, GRAVITY_CLASS_MAP_NAME)) return GravityEnv.P_ClsMap;
		if(sstreq(name, GRAVITY_CLASS_RANGE_NAME)) return GravityEnv.P_ClsRange;
		if(sstreq(name, GRAVITY_CLASS_UPVALUE_NAME)) return GravityEnv.P_ClsUpValue;
		if(sstreq(name, GRAVITY_CLASS_SYSTEM_NAME)) return GravityEnv.P_ClsSystem;
	}
	return NULL;
}

bool gravity_iscore_class(gravity_class_t * c) 
{
	// first check if it is a class
	if((c == GravityEnv.P_ClsObj) || (c == GravityEnv.P_ClsClass) || (c == GravityEnv.P_ClsBool) ||
	    (c == GravityEnv.P_ClsNull) || (c == GravityEnv.P_ClsInt) || (c == GravityEnv.P_ClsFloat) ||
	    (c == GravityEnv.P_ClsFunc) || (c == GravityEnv.P_ClsFiber) || (c == GravityEnv.P_ClsString) ||
	    (c == GravityEnv.P_ClsInstance) || (c == GravityEnv.P_ClsList) || (c == GravityEnv.P_ClsMap) ||
	    (c == GravityEnv.P_ClsRange) || (c == GravityEnv.P_ClsSystem) || (c == GravityEnv.P_ClsClosure) ||
	    (c == GravityEnv.P_ClsUpValue)) return true;

	// if class check is false then check for meta
	return ((c == gravity_class_get_meta(GravityEnv.P_ClsObj)) || (c == gravity_class_get_meta(GravityEnv.P_ClsClass)) ||
	       (c == gravity_class_get_meta(GravityEnv.P_ClsBool)) || (c == gravity_class_get_meta(GravityEnv.P_ClsNull)) ||
	       (c == gravity_class_get_meta(GravityEnv.P_ClsInt)) || (c == gravity_class_get_meta(GravityEnv.P_ClsFloat)) ||
	       (c == gravity_class_get_meta(GravityEnv.P_ClsFunc)) || (c == gravity_class_get_meta(GravityEnv.P_ClsFiber)) ||
	       (c == gravity_class_get_meta(GravityEnv.P_ClsString)) || (c == gravity_class_get_meta(GravityEnv.P_ClsInstance)) ||
	       (c == gravity_class_get_meta(GravityEnv.P_ClsList)) || (c == gravity_class_get_meta(GravityEnv.P_ClsMap)) ||
	       (c == gravity_class_get_meta(GravityEnv.P_ClsRange)) || (c == gravity_class_get_meta(GravityEnv.P_ClsSystem)) ||
	       (c == gravity_class_get_meta(GravityEnv.P_ClsClosure)) || (c == gravity_class_get_meta(GravityEnv.P_ClsUpValue)));
}