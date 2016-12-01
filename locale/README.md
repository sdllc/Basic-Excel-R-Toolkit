
BERT Localization
=================

Find the `locale` directory in the BERT install directory (usually this is in your 
AppData folder). Inside `locale`, copy the `template` folder and rename the copy. 
Either name it `dev` or use the name of your current locale. Locale names should be 
in the form `en-US`.

BERT will always load the `dev` locale before anything else.  If there's no `dev` locale, 
it will load the locale as reported by Windows (allowing user selection of locales is on 
the TODO list).  If that's not found, it will use the built-in defaults.

Instructions
------------

There are three files in the locale template.  Specific instructions for each file 
are listed below. If you modify the files, make sure to use UTF-8 encoding with no BOM.

Menus
-----

The file `menus.js` contains all the menus from the BERT console.  Update the `label` 
fields for each menu item.  You can optionally change the accelerator keys as well.

If a menu has a `role` but no `label`, it should be controlled by locale already, but 
you can optionally add a `label` if you feel the automatic label is not appropriate.

You can reload the BERT console to see changes to the javascript locale files.  Make 
sure that the menu option View -> Developer -> Allow Reloading is checked, then you can
use Ctrl+R to reload.  Any javascript errors will be displayed in the electron developer 
console (`Control+Shift+I` from the BERT Console).

Messages
--------

The `messages.js` file contains strings that are used in the console and the editor. 
You can reload the console to see changes (see above).

Ribbon Menu
-----------

The ribbon menu template is the XML file used in the Excel Add-ins tab.  Update the
`label` and `supertip` fields.

Unfortunately we can't hot-reload the ribbon template, so the only way to see changes 
is to exit Excel and restart.  If there is an error in the XML, the add-in menu will 
simply not display.  Unfortunately we can't provide error information.  Most editors 
should be able to validate XML before you test it.






