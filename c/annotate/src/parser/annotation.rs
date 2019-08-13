/// Parses a splitability annotation into string components.

use nom::*;

use super::ident;

/// A parsed split type from an annotation.
///
/// A split type is a name and a set of arguments with which the type is instantiated.
#[derive(Debug, Clone, PartialEq, Eq, Hash)]
pub enum ParsedSplitType {
    Broadcast,
    Generic(String),
    Named(ParsedNamedSplitType),
}

impl ParsedSplitType {
    /// Returns a new named split type with the give name and arguments.
    fn new_named<T: Into<String>>(name: T, arguments: Vec<T>) -> ParsedSplitType {
        ParsedSplitType::Named(
            ParsedNamedSplitType {
                name: name.into(),
                arguments: arguments.into_iter().map(|t| t.into()).collect(),
            })
    }
}

/// A parsed named split type from an annotation.
///
/// A split type is a name and a set of arguments with which the type is instantiated.
#[derive(Debug, Clone, PartialEq, Eq, Hash)]
pub struct ParsedNamedSplitType {
    /// The split type name.
    pub name: String,
    /// The arguments passed to instantiate the split type.
    pub arguments: Vec<String>,
}

/// A parsed parameter from an annotation.
///
/// An annotation is a name and its corresponding split type.
#[derive(Debug, Clone, PartialEq, Eq, Hash)]
pub struct ParsedParameter {
    /// The parameter name.
    pub name: String,
    /// The parameter type.
    pub ty: ParsedSplitType,
}

impl ParsedParameter {
    fn new<T: Into<String>>(name: T, ty: ParsedSplitType) -> ParsedParameter {
        ParsedParameter {
            name: name.into(),
            ty: ty,
        }
    }
}

/// A parsed annotation.
///
/// An annotation is a set of parameters and a set of defaults for generic parameters.
#[derive(Debug, Clone, PartialEq, Eq, Hash)]
pub struct ParsedAnnotation {
    /// ParsedParameters in the annotation.
    pub parameters: Vec<ParsedParameter>,
    /// Return type defined in the annotation.
    ///
    /// This must be provided if the C function return type is not `void`.
    pub return_type: Option<ParsedSplitType>,
    /// The default values for generics in the annotation.
    pub defaults: Vec<ParsedParameter>,
}

/// Parses a single split type.
named_complete!(
    parse_split_type<ParsedSplitType>,
    alt!(
        map!(
            ws!(do_parse!(
                name: ident >>
                args: delimited!(
                    char!('('),
                    separated_list_complete!(ws!(char!(',')), ident),
                    char!(')')
                ) >>
                (name, args)
            )),
            |n: (String, Vec<String>)| ParsedSplitType::new_named(n.0, n.1)
        ) |
        map!(ws!(tag_s!("broadcast")), |_| ParsedSplitType::Broadcast) |
        map!(ws!(ident), |n: String| ParsedSplitType::Generic(n))
    )
);

/// Parses a split type parameter, which is a name followed by a type.
named_complete!(
    parse_annotation_parameter<ParsedParameter>,
    map!(
        ws!(do_parse!(
                variable: ident >>
                char!(':') >>
                ty: parse_split_type >>
                (variable, ty)
        )),
        |n: (String, ParsedSplitType)| ParsedParameter::new(n.0, n.1)
    )
);

/// Parses a single splitability annotation.
named_complete!(
    pub parse_annotation<ParsedAnnotation>,
    map!(
        ws!(do_parse!(
                tag!("@sa") >>
                parameters: delimited!(
                    char!('('),
                    separated_list_complete!(ws!(char!(',')), parse_annotation_parameter),
                    char!(')')
                ) >>
                return_type: opt!(complete!(do_parse!(
                            tag_s!("->") >>
                            ty: parse_split_type >>
                            (ty)
                ))) >>
                defaults: opt!(complete!(do_parse!(
                            tag!("default") >>
                            defaults: separated_nonempty_list_complete!(
                                ws!(char!(',')),
                                parse_annotation_parameter
                            ) >>
                            (defaults)
                ))) >>
                (parameters, return_type, defaults)
        )),
        |n: (Vec<ParsedParameter>, Option<ParsedSplitType>, Option<Vec<ParsedParameter>>)| {
            ParsedAnnotation {
                parameters: n.0,
                return_type: n.1,
                defaults: n.2.unwrap_or(vec![]),
            }
        }
    )
);

#[cfg(test)]
use nom::types::CompleteByteSlice;

#[cfg(test)]
fn check_split_type(input: &str, expected: Option<ParsedSplitType>) {
    let result = parse_split_type(CompleteByteSlice(input.as_bytes()));
    if let Some(expected) = expected {
        assert_eq!(result.unwrap().1, expected);
    } else {
        result.expect_err("Expected an error");
    }
}

#[cfg(test)]
fn check_parameter(input: &str, expected: Option<ParsedParameter>) {
    let result = parse_annotation_parameter(CompleteByteSlice(input.as_bytes()));
    if let Some(expected) = expected {
        assert_eq!(result.unwrap().1, expected);
    } else {
        result.expect_err("Expected an error");
    }
}

#[test]
fn parse_split_type_no_args() {
    check_split_type("TestSplit()", Some(ParsedSplitType::new_named("TestSplit", vec![])));
}

#[test]
fn parse_split_type_one_arg() {
    check_split_type("TestSplit(n)", Some(ParsedSplitType::new_named("TestSplit", vec!["n"])));
}

#[test]
fn parse_split_type_broadcast() {
    check_split_type("broadcast", Some(ParsedSplitType::Broadcast));
}

#[test]
fn parse_split_type_generic() {
    check_split_type("S", Some(ParsedSplitType::Generic("S".to_string())));
}

#[test]
fn parse_split_two_args() {
    check_split_type("TestSplit(a, b)", Some(ParsedSplitType::new_named("TestSplit", vec!["a", "b"])));
}

#[test]
fn parse_split_type_whitespace() {
    check_split_type("   TestSplit ( )  ", Some(ParsedSplitType::new_named("TestSplit", vec![])));
}

#[test]
fn parse_split_type_arg_whitespace() {
    check_split_type("TestSplit(a,    b  )", Some(ParsedSplitType::new_named("TestSplit", vec!["a", "b"])));
}

#[test]
fn parse_parameter_simple() {
    let ty = ParsedSplitType::new_named("TestSplit", vec!["a"]);
    check_parameter("n: TestSplit(a)", Some(ParsedParameter::new("n", ty)));
}

#[test]
fn parse_parameter_underscore() {
    let ty = ParsedSplitType::new_named("TestSplit", vec!["a"]);
    check_parameter("arg_name: TestSplit(a)", Some(ParsedParameter::new("arg_name", ty)));
}

#[test]
fn parse_parameter_whitepsace() {
    let ty = ParsedSplitType::new_named("TestSplit", vec!["a"]);
    check_parameter("   n:    TestSplit(a)   ", Some(ParsedParameter::new("n", ty)));
}

#[test]
fn parse_parameter_extra_semicolon() {
    check_parameter("n:: TestSplit(a)", None);
}
