Build your own Lisp
===================

http://buildyourownlisp.com

About
-----

This is the HTML and website code for the book of the above title.

Corrections / Edits / Contributions Welcome

`contact@theorangeduck.com`

Book contents licensed under Creative Commons Attribution-NonCommercial-ShareAlike 3.0

http://creativecommons.org/licenses/by-nc-sa/3.0/

Source code licensed under BSD3

https://opensource.org/license/bsd-3-clause/


Running
-------

You can't just browse the raw HTML files of the site. The links wont work, and it wont have a proper header or footer. If you want to run this website locally, you should install Flask and run the website as follows.

```
pip install Flask cachelib
python lispy.py
```

You can specify port via `$PORT`.

```
env PORT=5000 python lispy.py
```

This will serve the site locally at `http://127.0.0.1:5000/`. You can browse it from there.

Or, if you have docker installed, and don't want to install the dependencies locally, run:
```
docker build --tag buildyourownlisp --build-arg PORT=5000 .
docker run --rm --publish 5000:5000 buildyourownlisp
```
Or, perhaps, just let `make` take care of it all with `make [PORT=5000] byol-docker`