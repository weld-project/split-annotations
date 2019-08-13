//! Parser for annotation files.

use composer::*;
use composer::Result as ComposerResult;

use log::*;

use nom::*;
use nom::types::CompleteByteSlice;

use std::collections::HashMap;
use std::str;

/// A macro to define top-level parses.
///
/// See https://github.com/Geal/nom/issues/790 with CompleteByteSlice
macro_rules! named_complete {
    ($name:ident<$t:ty>, $submac:ident!( $($args:tt)* )) => (
        fn $name( i: nom::types::CompleteByteSlice ) -> nom::IResult<nom::types::CompleteByteSlice, $t, u32> {
            $submac!(i, $($args)*)
        }
    );
    (pub $name:ident<$t:ty>, $submac:ident!( $($args:tt)* )) => (
        pub fn $name( i: nom::types::CompleteByteSlice ) -> nom::IResult<nom::types::CompleteByteSlice, $t, u32> {
            $submac!(i, $($args)*)
        }
    )
}

/// Parses an identifier name.
///
/// Identifiers are alphanumeric strings that may have underscores.
named_complete!(
    ident<String>,
    map!(
        // TODO first token shouldn't be a number.
        take_while1!(|b| nom::is_alphanumeric(b) || b == b'_'),
        |n: CompleteByteSlice| String::from(str::from_utf8(&n).unwrap())
    )
);

mod annotation;
mod cdecl;

use annotation::{parse_annotation, ParsedAnnotation, ParsedSplitType};
use cdecl::parse_c_decl;

/// A parsed annotation and its associated functions.
#[derive(Debug, Clone)]
struct Parsed {
    annotation: ParsedAnnotation,
    decls: Vec<CDecl>,
}

impl From<(ParsedAnnotation, Vec<CDecl>)> for Parsed {
    fn from(pair: (ParsedAnnotation, Vec<CDecl>)) -> Parsed {
        Parsed {
            annotation: pair.0,
            decls: pair.1
        }
    }
}

/// A trait for converting a parsed string-based representation into a structured one.
trait FromParsed {
    fn from_parsed(annotation: ParsedAnnotation, decl: CDecl) -> ComposerResult<Self>
        where Self: std::marker::Sized;
}

impl From<ParsedSplitType> for SplitType {
    fn from(parsed: ParsedSplitType) -> SplitType {
        match parsed {
            ParsedSplitType::Broadcast => SplitType::Broadcast { runtime: None, },
            ParsedSplitType::Generic(name) => SplitType::Generic { name },
            ParsedSplitType::Named(named) => {
                SplitType::Named {
                    name: named.name,
                    arguments: named.arguments.len(),
                    runtime: None,
                }
            }
        }
    }
}


/// Converts a list of arguments in a split type to a list of positions.
///
/// Parameters with duplicate names point to the same position.
fn args_to_positions(names: &Vec<&str>, ty: &ParsedSplitType) -> ComposerResult<Option<Vec<usize>>> {
    let args = if let ParsedSplitType::Named(ref named) = *ty {
        Some(&named.arguments)
    } else {
        None
    };

    args.map(|args| { 
        args.iter().map(|arg| {
            names.iter()
                .position(|x| x == arg)
                .ok_or(composer::Error::new(format!("Parameter {} not found.", arg)))
        }).collect::<Result<Vec<_>>>()
    }).transpose()
}

impl FromParsed for Annotation {
    /// Converts a single parsed annotation and single C declaration into `Self`.
    fn from_parsed(annotation: ParsedAnnotation, decl: CDecl) -> ComposerResult<Self> {

        debug!("parsed annotation: {:?}, decl: {}", annotation, decl);

        if annotation.parameters.len() != decl.arguments.len() {
            return composer_err!("Number of annotation types must equal number of args in declaration.");
        }

        if annotation.return_type.is_none() && decl.return_type != "void" {
            return composer_err!("Annotation over function with non-void return type must have return type in annotation.");
        }

        if annotation.return_type.is_some() && decl.return_type == "void" {
            return composer_err!("Annotation over function with void return type cannot have return type in annotation.");
        }

        // Extract the parameter names.
        let mut annotation_param_names: Vec<_> = annotation.parameters.iter()
            .map(|param| param.name.as_str())
            .collect();

        // Make sure the parameter names are unique.
        let length = annotation_param_names.len();
        annotation_param_names.dedup();

        if length != annotation_param_names.len() {
            return composer_err!("Parameter names in annotation must be unique.");
        }

        // The final result is reversed (so the arguments for the first split type appear last in
        // the list), so we can pop off elements in O(1) time in the following step.
        let mut arguments: Vec<_> = annotation.parameters.iter()
            .map(|param| args_to_positions(&annotation_param_names, &param.ty))
            .rev()
            .collect::<Result<_>>()?;

        let return_param = if let Some(ref return_type) = annotation.return_type {
            Some(SplitTypeParameter {
                arguments: args_to_positions(&annotation_param_names, return_type)?.unwrap_or(vec![]),
                ty: return_type.clone().into()
            })
        } else {
            None
        };

        // Construct the split type parameter.
        //
        // We first construct each split type, and then add it to a parameter.  We use the argument
        // indices constructed in the previous step.
        let parameters: Vec<_> = annotation.parameters.into_iter()
            .map(|param| param.ty)
            .map(|ty| ty.into())
            .map(|ty| SplitTypeParameter {
                ty: ty,
                arguments: arguments.pop().unwrap().unwrap_or(vec![]),
            })
            .collect();


        let annotation = Annotation {
            function: decl,
            return_param: return_param,
            params: parameters,
            defaults: HashMap::new(),
        };
        debug!("annotation: {:?}", annotation);
        Ok(annotation)
    }
}

named_complete!(
    parse_all<Vec<Parsed>>,
    map!(
        ws!(many1!(do_parse!(
                    annotation: parse_annotation >>
                    functions: delimited!(
                        char!('{'),
                        many1!(parse_c_decl),
                        char!('}')
                    ) >>
                    (annotation, functions)
        ))),
        |v| v.into_iter().map(|v| Parsed::from(v)).collect()
    )
);

/// Parses an annotation file into one or more annotations and declarations.
pub fn parse(s: &str) -> ComposerResult<Vec<Annotation>> {
    let parsed = parse_all(CompleteByteSlice(s.as_bytes()))
        .map(|v| v.1)
        .map_err(|_e| Error::new("Parse error"))?;

    parsed.into_iter()
        .flat_map(|parsed| {
            let annotation = parsed.annotation.clone();
            parsed.decls.into_iter()
                .map(move |decl| Annotation::from_parsed(annotation.clone(), decl))
        })
        .collect()
}
