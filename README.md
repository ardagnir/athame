Athame
======

Athame is a patch for your shell to add full Vim support by routing your keystrokes through an actual Vim process.

![Demo](http://i.imgur.com/MZCL1Vi.gif)

**Don't most shells already come with a vi-mode?**

Yes, and if you're fine with a basic vi imitation designed by a bunch of Emacs users, feel free to use it. ...but for the crazy Vim fanatics who sacrifice goats to the modal gods, Athame gives you the full power of Vim.

**This is alpha-quality software. It has bugs. Use at your own risk.**

##Requirements
- Athame requires Vim (your version needs to have +clientserver).
- Athame works best in GNU/Linux.

##Installation
**Step 1:** Clone this repo recursively.

    git clone --recursive http://github.com/ardagnir/athame
    cd athame

**Step 2:** Run the appropriate setup script

    ./readline_athame_setup.sh

Or

    ./zsh_athame_setup.sh

*Note: If you want a more manual/custom install, read the setup script for a list of commands. It's very simple.*

**Step 3:** Copy the default athamerc. This is optional but **strongly recommended.**

    cp athamerc ~/.athamerc

*Note: ~/.athamerc is like ~/.vimrc but for athame only. Add any athame-specific Vim customization here.*

##How to Use
Athame stores your Vim history in a Vim buffer with an empty line at the bottom and displays the line(s) of the cursor/highlighted text. Every key you type is sent to Vim and operates on this buffer. This allows you to use j/k/arrows to transverse history. Since this is Vim, you can use ? to search back and / to search forwards. The buffer is rebuilt from your new history after each command, so don't worry about destructive commands.

Unless you are using the Vim commandline(:,/,?), tabs and carriage returns are carried out by standard readline/zsh code.

##License

GPL v3
