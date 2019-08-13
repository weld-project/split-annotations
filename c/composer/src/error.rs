//! Errors in Composer.

use std::error;
use std::fmt;

/// Macro for creating an error.
#[macro_export]
macro_rules! composer_err {
    ( $($arg:tt)* ) => ({
        ::std::result::Result::Err($crate::Error::new(format!($($arg)*)))
    })
}

/// Errors produced by the annotation system.
#[derive(Debug, Clone)]
pub struct Error(String);

/// A custom result type for functions in this library.
pub type Result<T> = std::result::Result<T, Error>;

impl Error {
    pub fn new<T: Into<String>>(description: T) -> Error {
        Error(description.into())
    }
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "{}", self.0)
    }
}

impl error::Error for Error {
    fn cause(&self) -> Option<&error::Error> {
        None
    }
}

impl From<String> for Error {
    fn from(e: String) -> Error {
        Error(e)
    }
}

impl From<Error> for std::io::Error {
    fn from(e: Error) -> std::io::Error {
        std::io::Error::new(std::io::ErrorKind::Other, e.to_string())
    }
}

impl<'a> From<&'a str> for Error {
    fn from(e: &'a str) -> Error {
        Error(String::from(e))
    }
}
