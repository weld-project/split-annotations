//! Shared utility functions.

/// Joins a an iterator of strings using a delimiter.
pub fn join<'a>(start: impl Into<String>,
                sep: impl Into<&'a str>,
                strings: impl std::iter::Iterator<Item=String>) -> String {

    let sep = sep.into();
    strings.enumerate().fold(start.into(), |mut buf, (i, val)| {
        if i > 0 {
            buf.push_str(sep);
        }
        buf.push_str(&val);
        buf
    })
}
