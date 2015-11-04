Athame
======

Athame patches your shell to add full Vim support by routing your keystrokes through an actual Vim process. Athame can currently be used to patch readline (used by bash, gdb, python, etc) and/or zsh (which doesn't use readline).

![Demo](http://i.imgur.com/MZCL1Vi.gif)

**Don't most shells already come with a vi-mode?**

Yes, and if you're fine with basic vi imitations designed by a bunch of Emacs users, feel free to use them. ...but for the crazy Vim fanatics who sacrifice goats to the modal gods, Athame gives you the full power of Vim.

**This is alpha-quality software. It has bugs. Use at your own risk.**

##Requirements
- Athame requires Vim (your version needs to have [+clientserver](#setting-up-vim-with-clientserver) support).
- Athame works best in GNU/Linux.
- Athame also works on OSX.
- Athame requires an X display for communicating with Vim. (Your patched shell will still work without X, it just won't use Vim.)

##Think Before you Begin
Athame *probably* won't break your shell...

...but if it does, do you have a way to fix it that doesn't involve typing commands into the now-broken shell?

##Download
Clone this repo recursively:

    git clone --recursive http://github.com/ardagnir/athame

##Setting up Athame Readline
**1.** Run the setup script

    cd athame
    ./readline_athame_setup.sh

**2.** If you have an old version of athame and a ~/.athamerc file that doesn't source /etc/athamerc, check /etc/athamerc for changes.

*Notes:*
- *If this doesn't work, you may be using a distro (like Debian or Ubuntu) that doesn't use the system readline. See [building bash with system readline](#setting-up-bash-to-use-athame-readline)*
- *You can add the --nobuild flag to the setup script if you want to configure/build/install yourself*
- *You can change what vim binary is used by passing --vimbin=/path/to/vim to the setup script*


##Setting up Athame Zsh
**1.** Run the setup script

    cd athame
    ./zsh_athame_setup.sh

**2.** If you have an old version of athame and a ~/.athamerc file that doesn't source /etc/athamerc, check /etc/athamerc for changes.**

*Note:*
- *You can add the --nobuild flag to the setup script if you want to configure/build/install yourself*
- *You can change what vim binary is used by passing --vimbin=/path/to/vim to the setup script*

##Setting up Bash to use Athame Readline
Some distros (like Debian and Ubuntu) don't setup bash to use the system readline.
To check if this is the case, run:

    ldd $(which bash) | grep readline

If nothing shows up, you have to setup bash to use the system readline. If you're too lazy, you can run this script:

    ./bash_readline_setup.sh

If something shows up in `ldd`, but it isn't pointing at `/usr/lib/libreadline.so.6` (that's where Athame installs to by default), you need to move Athame's readline to whatever location it's looking in.

##FAQ
####How do I use this?
It's just like Vim. Imagine your history is stored inside a vim buffer (because it is!) with a blank line at the bottom. Your cursor starts on that blank line each time readline is called.
Unles you're in command mode, tabs and carriage returns are handled by readline/zsh.

Some commands (there's no specific code for these, it's just vim):

- j: go back a line in histor y
- k: go forward a line in history
- ?: search history (and current line) backwards (better if you have :set incsearch enabled)
- /: search history (and current line) forwards (better if you have :set incsearch enabled)
- cc: clear current line and go to insert mode

The default athamerc modifies up/down arrows in insert mode to jump to the end of the line, like in a normal shell, but you can change this.

You can also enable history searching arrows in the athamerc. (If you enable this and you use arrows, it searches for text that begins with what you've typed. It's like vim's command mode.)


####? and / are reversed from bash's vi-mode. Why?
Bash's vi-mode has ? go down and / go up. In actual Vim (and thus, Athame) they go the other way.

####Why does Athame start in insert/normal mode?
The default athamerc includes "startinsert" to make Athame start in insert mode. If you remove that line, it will start in normal mode.

####Why is my Athame slow?
Athame should be very fast, but it will slow down if your Vim setup is slow or if you have invalid vimscript in either your athamerc or vimrc. Try using a clean vimrc/athamerc and see if it speeds up. If not, file an issue.

####I installed Athame for Readline, but it isn't doing anything!
Are you using the [system readline?](#setting-up-bash-to-use-athame-readline)

####I got the error "Couldn't load vim"
Do you have a program called vim? (an alias is not good enough)

    which vim

Does this version of vim have [+clientserver support](#setting-up-vim-with-clientserver)

####How do I disable/uninstall Athame?
To temporarily disable athame, `export ATHAME_ENABLED=0` or run the setup script with a `noathame` flag.

To get rid of Athame completly, you should probably just replace it with the non-patched version of readline/zsh from your distro.

Or if you're cool with not having a shell, you can uninstall it by cding into the readline or zsh directory and typing `sudo make uninstall`.

Depending on your approach, you may want to manually remove `/usr/lib/athame*` and `/etc/athamerc` as well.

####Why do the Up/Down make the cursor jump to the end of the line?
This happens in insert mode and is one of several settings enabled in the default athamerc to make Athame more like a normal shell. Feel free to comment it out.

####How do I disable/change the way Athame shows vim's mode?
By default Athame shows the Vim mode at the bottom of the screen. This can be disabled using `export ATHAME_SHOW_MODE=0`

Athame stores the current mode in the `ATHAME_VIM_MODE` environment variable. You can use this to display the Vim mode yourself. See https://github.com/ardagnir/athame/issues/21 for an example using powerline.

Similarly, Vim commands are displayed at the bottom of the screen by default. These can be disabled using `export ATHAME_SHOW_COMMAND=0` and accessed using `ATHAME_VIM_COMMAND`

####Why don't arrow keys work when I add Athame to my convoluted Zsh setup?
Because arrow keys are evil! Just kidding. :P

Ohmyzsh and some other zsh setups put your terminal into application mode to help their keymaps. Athame doesn't like this, and besides, you're using Vim as your keymap. You can override this by adding the following to the end of your ~/.zshrc:

    function zle-line-init(){
      echoti rmkx
    }
    zle -N zle-line-init

####Why doesn't Athame work on my hipster terminal? It has this supercool terminfo that you've probably never heard of.
I hardcoded all the terminal codes. Sorry, I was lazy. Athame only works on xterm-like terminals.

####Does Athame work with Neovim?
Neovim doesn't support vim-style remote communication yet. The Neovim devs are documenting this in [Neovim issue 1750](https://github.com/neovim/neovim/issues/1750). If you want to use Neovim with Athame, you should consider helping them out. It sounds like most of the functionality is already there and just needs to be exposed in a backwards-compatible manner.

####Does Athame work on windows? I have Cygwin!
Haha, no.

####Does Athame support multibyte characters?
For readline it does. For zsh, any multibyte characters (except for the prompt) will cause trouble.

##Setting up vim with clientserver
You can test your vim's clientserver support by running:

    vim --version | grep clientserver

If you see +clientserver, that's good enough to run athame. There will still be a small bit of functionality missing from some plugins(anything that calls input()) if your vim is compiled with gui support.

Your distro's full vim version should have +clientserver support, but if you want to build vim yourself, this is the minimum setup for full athame functionality:

    hg clone https://vim.google.com/hg/ vim
    cd vim
    ./configure --with-features=huge --disable-gui
    make
    sudo make install

##Bugs
- Missing multibyte and right-prompt support for Zsh.
- Tab indentation doesn't work in python2. (You can indent with spaces though)
- See [issues](https://github.com/ardagnir/athame/issues) for more.
- If you see a bug without an issue, create one.

##License

GPL v3
