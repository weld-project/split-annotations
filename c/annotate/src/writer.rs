// Implements a struct that uses an annotation to generate a C header file.

use composer::*;
use composer::util::join;

use std::collections::HashSet;
use serde_json;

use subprocess::{Popen, PopenConfig, Redirection};

/// A trait that defines the function names associated with splitters.
trait SplitTypeFunctions {
    /// Returns the name of the splitter initialization function.
    fn splitter_init_func(&self) -> String; 

    /// Returns the name of the struct passed to the initializer function.
    fn splitter_init_arg_struct(&self) -> String;

    /// Returns the name of the splitter "next value" function.
    fn splitter_next_func(&self) -> String; 

    /// Returns the name of the splitter's struct type name in C.
    fn splitter_c_type(&self) -> String; 
}

impl SplitTypeFunctions for SplitType {
    fn splitter_init_func(&self) -> String {
        format!("{}_new", self.name().unwrap())
    }

    fn splitter_init_arg_struct(&self) -> String {
        format!("struct {}_init_args", self.name().unwrap())
    }

    fn splitter_next_func(&self) -> String {
        format!("{}_next", self.name().unwrap())
    }

    fn splitter_c_type(&self) -> String {
        format!("struct {}", self.name().unwrap())
    }
}

/// Writer for the file that includes all the other headers.
pub struct IncludeHeaderWriter {
    files: Vec<String>,
    functions: Vec<String>,
    includes: Vec<String>,
    include_guard: String,
}

impl IncludeHeaderWriter {
    pub fn new(files: Vec<String>, functions: Vec<String>, includes: Vec<String>, include_guard: String) -> Self {
        IncludeHeaderWriter {
            files: files,
            functions: functions,
            includes: includes,
            include_guard: include_guard,
        }
    }

    pub fn write(&mut self) -> String {
        let files = self.files.iter().map(|f| format!("#include \"{}.h\"", f));
        let includes = self.includes.iter().map(|f| format!("#include \"{}.h\"", f));
        let functions = self.functions.iter().map(|f| format!("{};", f));

        let format = vec![
            format!("#ifndef _COMPOSER_{}_H_", self.include_guard.to_uppercase()),
            format!("#define _COMPOSER_{}_H_", self.include_guard.to_uppercase()),
            "#ifdef __cplusplus\nextern\"C\" {\n#endif".to_string(),
            "".to_string(),
            join("",  "\n", includes),
            join("",  "\n", files),
            "\n// Public API:".to_string(),
            join("", "\n", functions),
            "#ifdef __cplusplus\n}\n#endif".to_string(),
            "\n#endif".to_string()
        ].into_iter();
        join("", "\n", format)
    }
}

/// Writer for creating annotation header files.
pub struct AnnotationHeaderWriter {
    annotation: Annotation,
    prefix: String,
}

impl AnnotationHeaderWriter {
    /// Creates a new annotation writer.
    pub fn new(annotation: Annotation) -> Self {
        AnnotationHeaderWriter {
            annotation: annotation,
            prefix: "c_".to_string(),
        }
    }

    /// Writes a C header file as a string.
    pub fn write(&mut self) -> String {
        let externs = self.write_forward_decls();
        let generator = self.write_annotation_generator();
        let argstruct = self.write_argstruct();
        let callback = self.write_callback();
        let callable = self.write_replaced_function();
        let code = format!(
            include_str!("resources/file.c.template"),
            externs=externs,
            function_name=self.annotation.function.func_name.to_uppercase(),
            generator=generator,
            argstruct=argstruct,
            callback=callback,
            callable=callable
        );

        // Attempt to format the code with clang-format.
        let config = PopenConfig {
            stdin: Redirection::Pipe,
            stdout: Redirection::Pipe,
            stderr: Redirection:: Pipe, 
            ..Default::default()
        };

        Popen::create(&["clang-format"], config)
            .and_then(|mut p| p.communicate(Some(&code)))
            .ok()
            .map(|(out, _)| out.unwrap())
            .unwrap_or(code)
    }

    /// Returns a filename for the header being annotated.
    pub fn filename(&self) -> String {
        format!("{}_composer", self.annotation.function.func_name) 
    }

    /// Returns a C header for the replacement function exported by this writer.
    pub fn function_header(&self) -> String {
        let mut replacement_func = self.annotation.function.clone();
        replacement_func.func_name = format!("{}{}",
                                        &self.prefix,
                                        replacement_func.func_name);
        format!("{}", replacement_func)
    }

    /// Writes the forward declarations required by the generated header.
    ///
    /// This includes the replacement function that users of this header file should call,
    /// and declarations of the splitters (whose implementations should be in a different file).
    pub fn write_forward_decls(&mut self) -> String {

        let replacement_func = format!("{};", self.function_header());

        // Tracks generated splitters so we don't generate them twice.
        let mut generated = HashSet::new();

        // Extern define the splitter structs and functions.
        //
        // Non-named types are skipped.
        let splitters = self.annotation.params.iter()
                                            .filter(|param| param.ty.is_named())
                                            .enumerate()
                                            .map(|(i, param)| {

            if generated.contains(&param.ty) {
                // Filtered out later.
                return "".to_string()
            }

            generated.insert(param.ty.clone());

            // Generate the init function.
            //
            // The init function comes with a special wrapper struct that holds the arguments
            // required to initialize the splitter.
            let mut init_arg_struct_fields: Vec<_> = param.arguments.iter().map(|index| {
                self.annotation.function.arguments[*index].0.clone()
            })
            .enumerate()
            .map(|(i, v)| format!("{} _{};", v, i))
            .collect();

            // Prevents annoying warnings.
            if init_arg_struct_fields.len() == 0 {
                init_arg_struct_fields.push("char _0;".to_string());
            }

            let ref out_ty = self.annotation.function.arguments[i].0;

            let init_arg_struct = format!("{name} {{\n{fields}\n}};",
                                          name=param.ty.splitter_init_arg_struct(),
                                          fields=join("", "\n", init_arg_struct_fields.into_iter()));

            let init_func = format!("void* {func_name}({out_ty} * item_to_split, {arg_struct} * args, int64_t *num_items);",
                func_name=param.ty.splitter_init_func(),
                arg_struct=param.ty.splitter_init_arg_struct(),
                out_ty=out_ty,
            );

            // Generate the next function.
            let next_func = format!("SplitterStatus {func_name}(const void *splitter, int64_t start, int64_t end, {out_ty} *out);",
                func_name=param.ty.splitter_next_func(),
                out_ty=out_ty
            );

            let ifdef_guard = format!("\n#ifndef _{name}_DEFN_\n#define _{name}_DEFN_",
                        name=param.ty.name().unwrap().to_uppercase());

            let decl = vec![
                format!("\n// Definitions for {}.", param.ty.name().unwrap()),
                ifdef_guard,
                init_arg_struct,
                init_func,
                next_func,
                "#endif\n".to_string()
            ].into_iter();
            join("", "\n", decl)
        }).filter(|s| s.len() != 0);

        let decls = vec![
            "// Replacement function exported by this header.".to_string(),
            replacement_func,
            "\n// Function that this header replaces.".to_string(),
            // annotated_func,
            join("", "\n", splitters)
        ].into_iter();

        join("", "\n", decls)
    }

    /// Writes an argument struct that is used to pass arguments to the runtime.
    ///
    /// This struct is passed a pointer the callback function.
    pub fn write_argstruct(&mut self) -> String {
        let field_list = self.annotation.function.arguments.iter()
            .enumerate()
            .map(|(i, arg)| format!("{} _{};", CDecl::strip_type_qualifiers(&arg.0), i));
        let field_list = join("", "\n", field_list);

        format!(
            include_str!("resources/argstruct.c.template"),
            function_name=self.annotation.function.func_name,
            struct_field_list=field_list
        )
    }

    /// Write the callback invoked by the runtime.
    pub fn write_callback(&mut self) -> String {
        let arg_list = self.annotation.function.arguments.iter()
            .enumerate()
            .map(|(i, _)| format!("arg->_{}", i));
        let arg_list = join("", ",", arg_list);

        let return_value = if self.annotation.function.is_void() {
            ""
        } else {
            "intptr_t ret = (intptr_t)"
        };

        let final_return_value = if self.annotation.function.is_void() {
           "(intptr_t)NULL" 
        } else {
            "ret"
        };

        format!(
            include_str!("resources/callback.c.template"),
            function_name=self.annotation.function.func_name,
            callback_call_list=arg_list,
            return_value=return_value,
            final_return_value=final_return_value
        )
    }

    /// Writes the replacement function that users of this header should call.
    pub fn write_replaced_function(&mut self) -> String {
        // Construct the argument list (e.g., int _1, float* _2, ..)
        let argument_list = self.annotation.function.arguments.iter()
            .enumerate()
            .map(|(i, arg)| format!("{} _{}", arg.0, i));
        let argument_list = join("", ",", argument_list);

        // Constructs a list of arguments passed to a function call (e.g., _1, _2, ..)
        let function_call_args = self.annotation.function.arguments.iter()
            .enumerate()
            .map(|(i, _arg)| format!("_{}", i));
        let function_call_args = join("", ",", function_call_args);

        // Construct assignments from the arguments to the struct (e.g., v._1 = _1;)
        let argstruct_construct_list = self.annotation.function.arguments.iter()
            .enumerate()
            .map(|(i, _)| format!("v._{} = _{};", i, i));
        let argstruct_construct_list = join("", "\n", argstruct_construct_list);

        let (return_tag, has_return_value, register_return) = if self.annotation.function.is_void() {
            ("", "0", "")
        } else {
            ("return", "1", "intptr_t tag = ")
        };

        let return_line = if self.annotation.function.is_void() {
            "".to_string()
        } else {
            format!("return ({})(tag);", self.annotation.function.return_type)
        };

        format!(
            include_str!("resources/function.c.template"),
            prefix=&self.prefix,
            argument_list=argument_list,
            function_call_args=function_call_args,
            return_type=self.annotation.function.return_type,
            return_tag=return_tag,
            has_return_value=has_return_value,
            register_return=register_return,
            return_line=return_line,
            function_name=self.annotation.function.func_name,
            argstruct_construct_list=argstruct_construct_list
        )
    }

    /// Writes the function that sets up the annotation at runtime.
    pub fn write_annotation_generator(&mut self) -> String {
        let json = serde_json::to_string(&self.annotation).unwrap();
        let json = json.replace("\"", "\\\"");

        let split_type_info = self.annotation.params.iter().enumerate().map(|(i, param)| { 

            let (initializer, next) = match param.ty {
                SplitType::Named { .. } => {
                    (param.ty.splitter_init_func(), param.ty.splitter_next_func())
                },
                SplitType::Broadcast { .. } => ( "NULL".to_string(), "NULL".to_string() ),
                SplitType::Generic { .. } => unimplemented!("Generic split types are not implemented"),
            };

            // TODO: This probably won't work with generics...
            format!("SetSplitTypeInfo(s, {index}, (intptr_t){initializer}, (intptr_t){next}, sizeof({arg_type}));",
                index=i,
                initializer=initializer,
                next=next,
                arg_type=self.annotation.function.arguments[i].0
            )
        });
        let split_type_info = join("", "\n", split_type_info);

        format!(
            include_str!("resources/annotation.c.template"),
            function_name=self.annotation.function.func_name,
            annotation_json=json,
            set_split_type_info=split_type_info
        )
    }
}
