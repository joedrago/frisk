frisk
=====

For Windows users that are too lazy to use grep, too poor for PowerGrep, and too picky for grepWin.
If this feels anything like Visual Studio's Find in Files output, then I did it right. There should
be a somewhat recent build of the app in the Downloads section.

Notes
-----

Filespecs can have any number of stars or question marks, when in wildcard (non-regex) mode.

Both path and filespec can have a semicolon-delimited list in them.

No, I will not add a Browse button. Okay, maybe I will, but I won't use it.

I am also accepting cooler icons to replace the random one I found on an icon website.

I like Courier New.

Build Requirements:
-------------------

* Visual Studio 2008
* I think that is it.

Also, this is the best command template ever: 

    gvim.exe --remote-silent +!LINE! +zz "!FILENAME!"
    
(The command template represents the command that is run if a line in the output is double clicked.)
    
Build Requirements:
-------------------

Code released under the Boost license.