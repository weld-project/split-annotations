//! A command line tool for annotating C functions with splitability annotations.
//!
//! This tool takes as input an annotations file, which defines splitability annotations, and
//! outputs C header files that can be compiled with a normal C program. The header files define
//! functions that support the splitability annotation runtime.

// Parses the annotation file.
mod parser;
mod writer;

use env_logger;
use log::*;

use std::fs::{DirBuilder, File};
use std::io::{Read, Write};
use std::path::PathBuf;

use clap::{Arg, App};


fn main() -> std::io::Result<()> {
    let matches = App::new("annotate")
        .version("0.1.0")
        .author("Shoumik Palkar <shoumik@cs.stanford.edu>")
        .about("Command line tool for annotating C functions with splitability annotations")
        .arg(Arg::with_name("INPUT")
             .help("annotation file")
             .required(true)
             .index(1))
        .arg(Arg::with_name("dir")
             .short("d")
             .help("name of directory where output header files are written (default: 'generated')")
             .takes_value(true)
             .required(false))
        .arg(Arg::with_name("force")
             .short("f")
             .help("Forces generation, even if a directory with the specified name already exists.")
             .required(false))
        .arg(Arg::with_name("prefix")
             .short("p")
             .help("prefix for generated functions (default: 'c_')")
             .takes_value(true)
             .required(false))
        .arg(Arg::with_name("header")
             .short("n")
             .help("name for include file with all definitions (default: 'generated')")
             .takes_value(true)
             .required(false))
        .arg(Arg::with_name("includes")
             .short("i")
             .help("Includes (without '.h') that should be added at the top of the main header file.")
             .takes_value(true)
             .required(false))
        .get_matches();

    // Initialize logging.
    let mut builder = env_logger::Builder::from_default_env();
    builder.default_format_timestamp(true)
           .init();

    let input_file = matches.value_of("INPUT").unwrap();
    let mut input_file = File::open(input_file)?;

    let mut annotations = String::new();
    input_file.read_to_string(&mut annotations)?;
    
    // Convert the string annotation file into annotation objects.
    let annotations = parser::parse(&annotations)?;

    let path = matches.value_of("dir").unwrap_or("generated");

    // If force is enabled, remove the existing directory.
    if matches.is_present("force") {
        std::fs::remove_dir_all(path)?;
    }

    // Create a directory.
    DirBuilder::new().create(path)?;

    let mut files = vec![];
    let mut functions = vec![];

    for annotation in annotations {
        let mut writer = writer::AnnotationHeaderWriter::new(annotation);

        info!("Generating header for {}...", writer.function_header());

        let header = writer.write();

        files.push(writer.filename());
        functions.push(writer.function_header());

        let mut filename = PathBuf::new();
        filename.push(path);
        filename.push(writer.filename());
        filename.set_extension("h");

        let mut f = File::create(filename).unwrap();
        f.write(header.as_bytes())?;
    }

    let includes = matches.value_of("includes").unwrap_or("");
    let includes = includes.split(",")
                        .map(|e| e.to_string())
                        .filter(|e| e.len() != 0)
                        .collect();

    // Write the final header.
    let mut include_writer = writer::IncludeHeaderWriter::new(files, functions, includes, path.to_string());
    let header = include_writer.write();

    let mut filename = PathBuf::new();
    filename.push(path);
    filename.push(matches.value_of("include").unwrap_or("generated"));
    filename.set_extension("h");

    let mut f = File::create(filename).unwrap();
    f.write(header.as_bytes())?;

    Ok(())
}
