//! The composer runtime and its associated components.
//!
//! Most of the interesting stuff is in the `runtime` module. This contains both the FFI functions
//! that the wrapper functions call, as well as the runtime itself, which launches parallel tasks.
//!
pub mod runtime;
pub mod util;

#[macro_use]
mod error;

pub use error::*;

use log::*;

use std::collections::HashMap;
use std::fmt;

use serde::{Serialize, Deserialize};
use serde_json;

// TODO change the initializer/next to actual function pointer types.
type FunctionPointer = usize;

/// A C declaration.
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct CDecl {
    pub return_type: String,
    pub func_name: String,
    pub arguments: Vec<(String, Option<String>)>,
}

impl CDecl {
    pub fn new<T>(return_type: T, func_name: T, arguments: Vec<(T, Option<T>)>) -> CDecl
        where T: Into<String> {
            CDecl {
                return_type: return_type.into(),
                func_name: func_name.into(),
                arguments: arguments.into_iter()
                    .map(|(ty, name)| (ty.into(), name.map(|n| n.into())))
                    .collect(),
            }
        }

    pub fn is_void(&self) -> bool {
        return self.return_type == "void"
    }

    pub fn strip_type_qualifiers(s: &str) -> String {
        s.replace("const ", "")
    }
}

impl fmt::Display for CDecl {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let arguments = self.arguments.iter().map(|(ty, name)| {
            format!("{}{}", &ty, 
                    name.as_ref()
                    .map(|n| format!(" {}", n))
                    .unwrap_or("".to_string())
            )
        });
        let arguments = util::join("", ", ", arguments);
        write!(f, "{} {}({})", self.return_type, self.func_name, arguments)
    }
}

/// Split type information that is known only at runtime.
///
/// This includes information such as function pointer values and the sizes of types, which can
/// only be known when the header files produced by the annotator tool are compiled.
#[derive(Debug,Clone,PartialEq,Eq,Hash,Serialize,Deserialize)]
pub struct SplitTypeRuntimeInfo {
    /// Function pointer to initializer.
    initializer: usize,
    /// Function pointer to retrieve next value.
    next: usize,
    /// Size of the value that the splitter splits.
    data_size: usize,
}

/// A split type.
///
/// Split types can either be braodcast (copy the value to each worker), generic (i.e., they can
/// take on any type) or named.
#[derive(Debug, Clone,PartialEq,Eq,Hash,Serialize,Deserialize)]
pub enum SplitType {
    Broadcast {
        runtime: Option<SplitTypeRuntimeInfo>,
    },
    Generic {
        name: String,
    },
    Named {
        name: String,
        arguments: usize,
        runtime: Option<SplitTypeRuntimeInfo>,
    },
}

impl SplitType {
    /// Returns the string name of this split type.
    pub fn name(&self) -> Option<&str> {
        match *self {
            SplitType::Broadcast { .. } => None,
            SplitType::Generic { ref name } => Some(name),
            SplitType::Named { ref name, .. } => Some(name),
        }
    }

    pub fn is_broadcast(&self) -> bool {
        match *self {
            SplitType::Broadcast { .. } => true,
            _ => false,
        }
    }

    pub fn is_named(&self) -> bool {
        match *self {
            SplitType::Named { .. } => true,
            _ => false,
        }
    }

    /// Returns the runtime information about the split type.
    ///
    /// Panics if this type is generic or if the runtime information is not present.
    pub fn runtime_info(&self) -> &SplitTypeRuntimeInfo {
        match *self {
            SplitType::Broadcast { ref runtime, .. } => runtime.as_ref().unwrap(),
            SplitType::Named { ref runtime, .. } => runtime.as_ref().unwrap(),
            SplitType::Generic { .. } => {
                panic!("Attempted to retrieve runtime information from generic type.")
            }
        }
    }
}

/// A parameter within the context of an annotation.
///
/// This struct defines the split type along with the arguments that are fed to the type to instantiate
/// it. Arguments are indices into the function argument list (e.g., `[1,2,4]` means that the first,
/// second, and fourth arguments should be passed to the split type initializer). Generic split
/// types should not have any arguments.
#[derive(Debug, Clone, PartialEq,Serialize,Deserialize)]
pub struct SplitTypeParameter {
    pub ty: SplitType,
    pub arguments: Vec<usize>,
}

/// A self-contained annotation over a C function.
#[derive(Debug, Clone, PartialEq,Serialize,Deserialize)]
pub struct Annotation {
    pub function: CDecl,
    pub params: Vec<SplitTypeParameter>,
    pub return_param: Option<SplitTypeParameter>,
    pub defaults: HashMap<String, SplitType>,
}

/// Entry points for parsing and creating annotations from strings.
impl Annotation {
    /// Parses an annotation from a JSON string.
    pub fn from_json(s: &str) -> Result<Annotation> {
        Ok(serde_json::from_str(s).unwrap())
    }

}

/// Methods for runtime instantiation of annotations.
///
/// These methods are called from the generated C code (via FFI).
impl Annotation {
    /// Sets the runtime information for an argument.
    fn set_type_runtime_info(&mut self,
                             index: usize,
                             rt: SplitTypeRuntimeInfo) -> Result<()> {
        let param = self.params.get_mut(index).unwrap();
        match param.ty {
            SplitType::Named { ref mut runtime, .. } | SplitType::Broadcast { ref mut runtime, .. } => {
                *runtime = Some(rt);
            }
            SplitType::Generic { .. } => {
                info!("instantiated {:?} with no runtime info.", self);
            }
        }
        Ok(())
    }
}
