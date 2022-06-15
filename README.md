# nee-editor
This is an minimal editor which provide some basic features including searching and syntax highlight. This editor is inspired by the excellent tutorial [Build your own text editor](https://viewsourcecode.org/snaptoken/kilo/index.html). I follow mostly the program structure from the tutorial but I also extended some additional functionalities.


## Usage
Open your terminal then compile the editor by make then you can get the executable file. Run the following command to execute the editor. 
```
user$ ./nee <filename>
```
Then the program would enter the raw mode which would temporarily disable some keyboard functionalities. Then you can edit the file by typing the keyboard to insert characters you want. You can try to press some keys then you would see the output shows up on your screen.

```
~
~
~
~
~
~
~
```
Here're some useful short-cut to use:
* `Ctrl` + `S` : Save file.
* `Ctrl` + `Q` : Quit the editor.
* `Ctrl` + `F` : Find the specific syntax in file.
  - `ESE` : Cancel search.
  - `ENTER` : Perform search.
  - `Arrows`: Navigate the cursor to `next` or `previous` fitting syntax.
* `PageUp/PageDown` : Scroll Up/Down.
* `Up/Down/Left/Right` : Move the current position of cursor.
* `Home/End` : Move the cursor to the begining/end of editing line.




## License

Nee editor is freely redistributable under the BSD 2 clause license. Use of
this source code is governed by a BSD-style license that can be found in the
LICENSE file.
