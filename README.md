<h2 align="center">cenv_toolkit</h2>
  
<p align="center">
    <img src="https://img.shields.io/badge/license-MIT-green?style=flat-square" alt="MIT License" />
  <img src="https://img.shields.io/github/last-commit/simon-danielsson/cenv_toolkit_toolkit/main?style=flat-square&color=blue" alt="Last commit" />
</p>
  
<p align="center">
  <a href="#info">Info</a> •
  <a href="#install">Install</a> •
  <a href="#usage">Usage</a> •
  <a href="#toolkit">Toolkit</a> •
  <a href="#license">License</a>
</p>  
  
---
<div id="info"></div>

## Info
  
cenv_toolkit assumes that everything required to build and maintain a C project should live within the codebase itself. It is an *opinionated* development environment built for developing small to medium sized C projects.
  
Prerequisites:  
- git  
- curl  
- a C compiler  
  
> [!IMPORTANT]  
> 1. No support for Windows. Only unix systems.
> 2. Since cenv_toolkit is heavily opinionated and built for my own specific workflow, I can't
> guarantee that this will function properly on your computer (or be enjoyable
> to use.)
  
cenv_toolkit relies on [nob.h](https://github.com/tsoding/nob.h) (a header-only
build-system) for compilation.  
  
---
<div id="install"></div>

## Install
  
[cenv_init.sh](./cenv_init.sh) functions as a bootstrap command that builds a new c project. Add it as an alias in your `.bashrc`:  
  
``` bash
# ~/.bashrc

# cinit script
alias cinit="$HOME/path/to/cenv_init.sh"

# this function lets you launch the cenv binary from anywhere
# within your cenv_toolkit project folder (assuming you have
# a .gitignore in its root)
cenv() {
    local dir="$(pwd)"
    while [[ "$dir" != "/" ]]; do
        if [[ -f "$dir/.gitignore" ]]; then
            (cd "$dir" && cenv "$@")
            return
        fi
        dir="$(dirname "$dir")"
    done
    return 1
}
```
  
---
<div id="usage"></div>
  
## Usage
  
Run cinit in your destination folder with the project name as an argument, then run the help command to get started:  
  
``` bash
cinit my_project
cd my_project
cenv_toolkit help
```
 
When you run `cenv_toolkit help` you will see the following commands:

``` terminal
cenv_toolkit debug
│ compile into and run from './build/debug' with debug options
╰ default command
cenv_toolkit release
╰ compile into and run from './build/release' with optimizations
cenv_toolkit test
│ compile into and run from './build/tests' directory with debug options
╰ the source folder used for this command is './tests'

cenv_toolkit doc
│ auto-generate docs from './src' and open in browser
╰ this command is still in the experimental stage
cenv_toolkit todo
╰ find and print all 'TODO' statements in codebase
cenv_toolkit update
│ update bundled cenv_toolkit tools and header-only libraries from their
╰ known upstream git sources - user-added dependencies are safely ignored
cenv_toolkit tidy
╰ clean up log, html, debug and object files
cenv_toolkit help
╰ display help

```
  
---
<div id="toolkit"></div>
  
## Toolkit
    
### cenv_toolkit todo
  
'cenv_toolkit todo' returns a formatted list of all 'TODO' statements found, each with a reference to file and line number. In addition to this, 'cenv_toolkit todo' collects entire 'TODO' paragraphs, not just lines.
  
### cenv_toolkit doc
  
cenv_toolkit comes bundled with its own auto-documentation tool that generates a static html page you can browse, similar to the 'cargo doc' system from Rust. The syntax is simple to understand and is explained within the following example code.
  
``` c
//! Math utilities
//!
//! Simple utilities for calculating numbers.
//! 
//! A good practice is to give every file in your project a header
//! like this one. 
//! <--- "//!" is the syntax used for file headers.
//! 
//! @important This is the file header comment.

// There are three tags you can use to spice up your documentation apart from
// headers and comments: @important, @param and @return. 
// 
// These tags are not bound to any specific rule or syntax, so you can 
// place whatever text you want after them - you can use as many tags as you 
// want in a single piece of documentation, with the caveat that they can not
// be multi-line.
// The cenv_toolkit parser doesn't care where the tags are so you can place a...
// @important hello
// ...tag anywhere and it will work the same as placing tags at the end or
// grouping tags together.

// cenv_toolkit is not using the standard "//" or "/**/" comments as doc comments 
// so that you, as the programmer, can be explicit about which comments you
// want to use as documentation and which should be used internally only


/// Used for setting factor in submult() function
#define MULT 5 // only adding a header is fine too

/// sum two integers
///
/// @param y int
/// @param x int
/// @return sum
int add(int y, int x) {
    return y + x;
}

/// y - x * MULT
/// The header is always whatever line is at the top,
/// while the description always comes afterwards.
/// Note that the header can only be a single line.
/// <-- "///" is comment syntax used for functions
/// @param two integers
/// @important this is only used once
/// @return sum
int submult(int y, int x) {
    return (y - x) * MULT;
}
```
### example generated cenv_toolkit documentation pages
  
![cenv_doc](./media/cenv_doc.png)
![cenv_doc2](./media/cenv_doc2.png)
  
---
<div id="license"></div>

## License
  
This project is licensed under the [MIT License](https://github.com/simon-danielsson/cenv_toolkit/blob/main/LICENSE).  
 
