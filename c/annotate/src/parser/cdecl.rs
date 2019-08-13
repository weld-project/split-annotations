//! Parsing for C function headers.

use composer::CDecl;

use nom::*;
use nom::types::CompleteByteSlice;

use std::str;

use super::ident;

/// Parses a C type.
///
/// This currently supports parsing any regular type, or types with pointers.
///
/// # Unsupported Features
///
/// * Function Pointers
/// * Fixed size arrays
named_complete!(
    pub parse_c_type<String>,
    map!(
        ws!(do_parse!(
                is_const: opt!(tag_s!("const")) >>
                tag: opt!(alt!(
                        tag_s!("struct") |
                        tag_s!("union")
                )) >>
                ty: ident >>
                pointers: many0!(char!('*')) >>
                (is_const, tag, ty, pointers)
        )),
        |(is_const, tag, ty, ptrs): (Option<CompleteByteSlice>, Option<CompleteByteSlice>, String, Vec<char>)| {
            let is_const = is_const.map(|ref v| str::from_utf8(v).unwrap());
            let tag = tag.map(|ref v| str::from_utf8(v).unwrap());

            let mut name = String::new();

            if let Some(is_const) = is_const {
                name.push_str(is_const);
                name.push(' ');
            }

            if let Some(tag) = tag {
                name.push_str(tag);
                name.push(' ');
            };

            name.push_str(&ty);

            for _ in 0..ptrs.len() {
                name.push('*');
            }
            name
        }
    )
);

/// Parses a C parameter, which is a type followed by an optional name.
named_complete!(
    parse_c_parameter<(String, Option<String>)>,
    ws!(do_parse!(
            ty: parse_c_type >>
            arg_name: opt!(ident) >>
            (ty, arg_name)
    ))
);


/// Parses a single C function declaration.
named_complete!(
    pub parse_c_decl<CDecl>,
    map!(
        ws!(do_parse!(
            return_type: parse_c_type >>
            func_name: ident >>
            arguments: delimited!(
                char!('('),
                separated_list_complete!(ws!(char!(',')), parse_c_parameter),
                char!(')')
            ) >>
            char!(';') >>
            (return_type, func_name, arguments)
        )),
        |n: (String, String, Vec<(String, Option<String>)>)| CDecl::new(n.0, n.1, n.2)
    )
);

#[cfg(test)]
fn check_type(input: &str, expected: Option<&str>) {
    let result = parse_c_type(CompleteByteSlice(input.as_bytes()));
    if let Some(expected) = expected {
        assert_eq!(expected, result.unwrap().1);
    } else {
        result.expect_err("Expected an error");
    }
}

#[cfg(test)]
fn check_parameter(input: &str, expected: Option<(&str, Option<&str>)>) {
    let result = parse_c_parameter(CompleteByteSlice(input.as_bytes()));
    if let Some(expected) = expected {
        let result = result.unwrap().1;
        assert_eq!(result, (String::from(expected.0), expected.1.map(|v| String::from(v))));
    } else {
        result.expect_err("Expected an error");
    }
}

#[cfg(test)]
fn check_decl(input: &str, expected: Option<CDecl>) {
    let result = parse_c_decl(CompleteByteSlice(input.as_bytes()));
    if let Some(expected) = expected {
        let result = result.unwrap().1;
        assert_eq!(result, expected);
    } else {
        result.expect_err("Expected an error");
    }
}

#[test]
fn simple_type() {
    check_type("int", Some("int")); 
}

#[test]
fn pointer_type() {
    check_type("int*", Some("int*")); 
}

#[test]
fn const_type() {
    check_type("const int", Some("const int")); 
}

#[test]
fn pointer_type_with_space() {
    check_type("int *", Some("int*")); 
}

#[test]
fn struct_type() {
    check_type("struct myStruct", Some("struct myStruct")); 
}

#[test]
fn const_struct_type() {
    check_type("const struct myStruct", Some("const struct myStruct")); 
}

#[test]
fn struct_pointer_type() {
    check_type("struct myStruct *", Some("struct myStruct*")); 
}

#[test]
fn const_struct_pointer_type() {
    check_type("const struct myStruct *", Some("const struct myStruct*")); 
}

#[test]
fn double_pointer_type() {
    check_type("int**", Some("int**")); 
}

#[test]
fn simple_parameter() {
    check_parameter("int x", Some(("int", Some("x"))))
}

#[test]
fn pointer_parameter() {
    check_parameter("int *x", Some(("int*", Some("x"))))
}

#[test]
fn struct_parameter() {
    check_parameter("struct foo *x", Some(("struct foo*", Some("x"))))
}

#[test]
fn no_name_parameter() {
    check_parameter("struct foo *", Some(("struct foo*", None)))
}

#[test]
fn incomplete_type_parameter() {
    check_parameter("struc foo *x", Some(("struc", Some("foo"))))
}

#[test]
fn basic_decl() {
    check_decl("int foo();", Some(CDecl::new("int", "foo", vec![])));
}

#[test]
fn decl_with_arg() {
    check_decl("int foo(int);", Some(CDecl::new("int", "foo", vec![("int", None)])));
}

#[test]
fn decl_with_many_args() {
    let args = vec![
        ("int", None),
        ("struct myStruct**", None),
        ("float*", None)
    ];
    let expected = CDecl::new("int", "foo", args);
    check_decl("int foo(int, struct myStruct **, float *);", Some(expected));
}

#[test]
fn decl_with_some_named_args() {
    let args = vec![
        ("int", None),
        ("struct myStruct**", Some("arg2")),
        ("float*", None)
    ];
    let expected = CDecl::new("int", "foo", args);
    check_decl("int foo(int, struct myStruct **arg2, float *);", Some(expected));
}

