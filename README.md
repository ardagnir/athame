Athame
======

Athame patches your shell to add full Vim support by routing your keystrokes through an actual Vim process. Athame can currently be used to patch readline (used by bash, gdb, python, etc) and/or zsh (which doesn't use readline).

![Demo](http://i.imgur.com/MZCL1Vi.gif)

**Don't most shells already come with a vi-mode?**

Yes, and if you're fine with basic vi imitations designed by a bunch of Emacs users, feel free to use them. ...but for the crazy Vim fanatics who sacrifice goats to the modal gods, Athame gives you the full power of Vim.

##Requirements
- Athame works best in GNU/Linux.
- Athame also works on OSX.

For vim-mode (Athame will act similarly to a normal shell if these are missing):
 - Vim (your version needs to have [+clientserver](#setting-up-vim-with-clientserver) support)
 - X (For linux, you probably have this already. For OSX, install XQuartz)

##Will Athame break my shell?
Athame *probably* won't break your shell. If anything goes wrong, it *should* fail into normal shell behavior. If, for example, you haven't started X, Athame will give a warning and act like a normal shell until you do.

...but there are no guarantees. You probably shouldn't install Athame on production systems.

##Download
Clone this repo recursively:

    git clone --recursive http://github.com/ardagnir/athame

##Setting up Athame Readline
**1.** Run the setup script

    cd athame
    ./readline_athame_setup.sh

**2.** If you have an old version of Athame and a ~/.athamerc file that doesn't source /etc/athamerc, check /etc/athamerc for changes.

*Notes:*
- *If this doesn't work, you may be using a distro (like Debian or Ubuntu) that doesn't use the system readline. See [building bash with system readline](#setting-up-bash-to-use-athame-readline)*
- *You can add the --nobuild flag to the setup script if you want to configure/build/install yourself*
- *You can change what Vim binary is used by passing --vimbin=/path/to/vim to the setup script*


##Setting up Athame Zsh
**1.** Run the setup script

    cd athame
    ./zsh_athame_setup.sh

**2.** If you have an old version of Athame and a ~/.athamerc file that doesn't source /etc/athamerc, check /etc/athamerc for changes.**

*Note:*
- *You can add the --nobuild flag to the setup script if you want to configure/build/install yourself*
- *You can change what Vim binary is used by passing --vimbin=/path/to/vim to the setup script*

##Setting up Bash to use Athame Readline
Some distros (like Debian and Ubuntu) don't setup bash to use the system readline.
To check if this is the case, run:

    ldd $(which bash) | grep readline

If nothing shows up, you have to setup bash to use the system readline. If you're too lazy, you can run this script:

    ./bash_readline_setup.sh

If something shows up in `ldd`, but it isn't pointing at `/usr/lib/libreadline.so.6` (that's where Athame installs to by default), you need to move Athame's readline to whatever location it's looking in.

##FAQ
####How do I use this?
It's just like Vim. Imagine your history is stored inside a Vim buffer (because it is!) with a blank line at the bottom. Your cursor starts on that blank line each time readline is called.
Unless you're in command mode, tabs and carriage returns are handled by readline/zsh.

Some commands (there's no specific code for these, it's just Vim):

- j: go back a line in history
- k: go forward a line in history
- ?: search history (and current line) backwards (better if you have :set incsearch enabled)
- /: search history (and current line) forwards (better if you have :set incsearch enabled)
- cc: clear current line and go to insert mode

The default athamerc modifies up/down arrows in insert mode to jump to the end of the line, like in a normal shell, but you can change this.

You can also enable history searching arrows in the athamerc. (If you enable this and you use arrows, it searches for text that begins with what you've typed. It's like Vim's command mode.)


####? and / are reversed from bash's vi-mode. Why?
Bash's vi-mode has ? go down and / go up. In actual Vim (and thus, Athame) they go the other way.

####Why does Athame start in insert/normal mode?
The default athamerc includes "startinsert" to make Athame start in insert mode. If you remove that line, it will start in normal mode.

####Why is my Athame slow?
Athame should be very fast, but it will slow down if your Vim setup is slow or if you have invalid vimscript in either your athamerc or vimrc. Try using a clean vimrc or athamerc and see if it speeds up. If not, file an issue.

####I installed Athame for Readline, but it isn't doing anything!
Are you using the [system readline?](#setting-up-bash-to-use-athame-readline)

####I got the error "Couldn't load vim path"
Is Vim at the correct path? You can change the path Athame looks at with the --vimbin flag.

####How do I disable/uninstall Athame?
To temporarily disable Athame, `export ATHAME_ENABLED=0`

To compile readline/zsh without the Athame patches, run the setup script with a `--noathame` flag.

To get rid of Athame completly, you should probably just replace it with the non-patched version of readline/zsh from your distro.

Or, if you're cool with not having a shell, you can uninstall it by cding into the readline or zsh directory and typing `sudo make uninstall`.

Depending on your approach, you may want to manually remove `/usr/lib/athame*` and `/etc/athamerc` as well.

####Why do the Up/Down arrows make the cursor jump to the end of the line?
This is done through vimscript in the athamerc to make Athame more like a normal shell. Feel free to comment it out.

####How do I disable/change the way Athame shows Vim's mode?
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

####Does Athame work with Neovim?
Neovim doesn't support vim-style remote communication yet. The Neovim devs are documenting this in [Neovim issue 1750](https://github.com/neovim/neovim/issues/1750). If you want to use Neovim with Athame, you should consider helping them out. It sounds like most of the functionality is already there and just needs to be exposed in a backwards-compatible manner.

####Why isn't there an Athame package for my favorite distro?
...because you haven't made one yet. The Athame setup script comes with a --nobuild flag so that you can build it however you want or your package can just apply the Athame patches itself.

####Does Athame work on windows? I have Cygwin!
Haha, no.

####This is awesome! Can I help?
The best way to help is to look at the issue section and submit patches to fix bugs.

If you have a shell that I'm missing, you can also try making a patch to communicate between it and Athame (see athame_readline.h and athame_zsh.h for the functions you need to implement).

####What about donations?
I'm not accepting any currently, but thanks.


##Setting up Vim with clientserver
You can test your Vim's clientserver support by running:

    vim --version | grep clientserver

If you see +clientserver, you can run Athame. (If you're using actual Vim. MacVim, for example, doesn't report this correctly and won't work for Athame.)

Your distro's full Vim version should have +clientserver support, but if you want to build vim yourself, this is the minimum setup for full Athame functionality:

    git clone https://github.com/vim/vim
    cd vim
    ./configure --with-features=huge
    make
    sudo make install

##Bugs
- Tab indentation doesn't work in the python2 shell. (You can indent with spaces though)
- See [issues](https://github.com/ardagnir/athame/issues) for more.
- If you see a bug without an issue, create one.

##License
GPL v3
