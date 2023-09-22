import os
import logging
import random
import datetime

from cachelib import MemcachedCache
from werkzeug.datastructures import ImmutableOrderedMultiDict

from flask import Flask, jsonify, request, send_file, redirect, url_for

try:
    # For Python 3.0 and later
    from urllib.request import urlopen
except ImportError:
    # Fall back to Python 2's urllib2
    from urllib2 import urlopen

pages = [
    'splash.html', 'contents.html', 'credits.html', 'faq.html', '404.html', 'purchase.html', 'test.html', 'invalid.html',
    'download.html',
    'chapter1_introduction.html',       'chapter2_installation.html',   'chapter3_basics.html',
    'chapter4_interactive_prompt.html', 'chapter5_languages.html',      'chapter6_parsing.html',
    'chapter7_evaluation.html',         'chapter8_error_handling.html', 'chapter9_s_expressions.html',
    'chapter10_q_expressions.html',     'chapter11_variables.html',     'chapter12_functions.html',
    'chapter13_conditionals.html',      'chapter14_strings.html',       'chapter15_standard_library.html',
    'chapter16_bonus_projects.html',

    'appendix_a_hand_rolled_parser.html',
]

titles = [
    'Learn C', 'Contents', 'Credits', 'Frequently Asked Questions', 'Page Missing', 'Buy Now!', 'Test', 'Invalid Download',
    'Download',
    'Introduction &bull; Chapter 1',       'Installation &bull; Chapter 2',   'Basics &bull; Chapter 3',
    'Interactive Prompt &bull; Chapter 4', 'languages &bull; Chapter 5',      'Parsing &bull; Chapter 6',
    'Evaluation &bull; Chapter 7',         'Error Handling &bull; Chapter 8', 'S-Expressions &bull; Chapter 9',
    'Q-Expressions &bull; Chapter 10',     'Variables &bull; Chapter 11',     'Functions &bull; Chapter 12',
    'Conditionals &bull; Chapter 13',      'Strings &bull; Chapter 14',       'Standard Library &bull; Chapter 15',
    'Bonus Projects &bull; Chapter 16',

    'Hand Rolled Parser &bull; Appendix A'
]

sources = [
    (), (), (), (), (), (), (), (),
    (),
    (), ('hello_world.c', ), (),
    ('prompt_unix.c', 'prompt_windows.c', 'prompt.c'), ('doge_code.c', 'doge_grammar.c'),
    ('parsing.c', ), ('evaluation.c', ), ('error_handling.c',), ('s_expressions.c',), ('q_expressions.c',),
    ('variables.c',), ('functions.c',), ('conditionals.c',), ('strings.c',), ('prelude.lspy',), (),
    ('hand_rolled_parser.c',)
]

header = """
<!DOCTYPE html>
<html>
  <head>
    <title>%s &bull; Build Your Own Lisp</title>
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <!-- Bootstrap -->
    <link href="/static/css/bootstrap.css" rel="stylesheet">
    <link href="/static/css/code.css" rel="stylesheet">
    <link href="/static/css/main.css" rel="stylesheet">
    <link rel="icon" type="image/png" href="/static/img/favicon.png" />

    <!-- HTML5 Shim and Respond.js IE8 support of HTML5 elements and media queries -->
    <!-- WARNING: Respond.js doesn't work if you view the page via file:// -->
    <!--[if lt IE 9]>
      <script src="https://oss.maxcdn.com/libs/html5shiv/3.7.0/html5shiv.js"></script>
      <script src="https://oss.maxcdn.com/libs/respond.js/1.3.0/respond.min.js"></script>
    <![endif]-->

  <script>
    (function(i,s,o,g,r,a,m){i['GoogleAnalyticsObject']=r;i[r]=i[r]||function(){
    (i[r].q=i[r].q||[]).push(arguments)},i[r].l=1*new Date();a=s.createElement(o),
    m=s.getElementsByTagName(o)[0];a.async=1;a.src=g;m.parentNode.insertBefore(a,m)
    })(window,document,'script','//www.google-analytics.com/analytics.js','ga');

    ga('create', 'UA-46885107-1', 'buildyourownlisp.com');
    ga('send', 'pageview');
  </script>
  <style>
  @media (max-width: 640px) {
      h4 { font-size: 90%%; }
      }
  </style>
  </head>
  <body>
  <div class="tiled-roof">
    <div class="container">
        <div class="row">
         <div class="col-xs-12">

"""

footer = """
         </div>
        </div>
    </div>

    <!-- jQuery (necessary for Bootstrap's JavaScript plugins) -->
    <script src="https://code.jquery.com/jquery.js"></script>
    <!-- Include all compiled plugins (below), or include individual files as needed -->
    <script src="/static/js/bootstrap.js"></script>

    <!-- Syntax Highlighting -->

    <script src="/static/js/rainbow.js"></script>
    <script src="/static/js/language/generic.js"></script>
    <script src="/static/js/language/c.js"></script>
    <script src="/static/js/language/lispy.js"></script>
  </div>
  </body>
</html>
"""

try:
    cache = MemcachedCache(['127.0.0.1:11211'])
except RuntimeError:

    class FakeCache:
        def get(self, k): return None
        def set(self, k, v, **kwargs): return None

    cache = FakeCache()

app  = Flask(__name__)

handler = logging.FileHandler(os.path.join(os.path.split(__file__)[0], 'error.log'))
handler.setLevel(logging.INFO)
app.logger.addHandler(handler)

#from flask.ext.basicauth import BasicAuth
#app.config['BASIC_AUTH_USERNAME'] = 'byol'
#app.config['BASIC_AUTH_PASSWORD'] = 'lovelace'
#app.config['BASIC_AUTH_FORCE'] = True
#app_auth = BasicAuth(app)

""" Page """

code_header = """
  <div class="panel panel-default">
    <div class="panel-heading">
      <h4 class="panel-title">
        <a data-toggle="collapse" data-parent="#accordion" href="#collapse%s">
          %s
        </a>
      </h4>
    </div>
    <div id="collapse%s" class="panel-collapse collapse">
      <div class="panel-body">
"""

code_footer = """
      </div>
    </div>
  </div>
"""

def code_html(codes):

    string = ('<div class="panel-group alert alert-warning" id="accordion">\n'
              '  <div class="panel panel-default">')

    for num, code in zip(('One', 'Two', 'Three', 'Four', 'Five'), codes):

        string += code_header % (num, code, num)

        if code.endswith('.lspy'):
            string += '<pre><code data-language=\'lispy\'>'
        else:
            string += '<pre><code data-language=\'c\'>'

        path = os.path.join(os.path.split(__file__)[0], 'src', code)

        with open(path, 'r') as f:
            contents = f.read()
            contents = contents.replace('&', '&amp;')
            contents = contents.replace('<', '&lt;')
            contents = contents.replace('>', '&gt;')
            string += contents

        string = string.rstrip() + '</code></pre>'
        string += code_footer + '\n\n'

    string += '</div>\n  </div>'

    return string

@app.route('/<page>')
def route_page(page):
    page = page + '.html'
    if not page in pages: page = '404.html'
    path = os.path.join(os.path.split(__file__)[0], page)

    index = pages.index(page)
    title = titles[index]
    codes = sources[index]

    contents = cache.get("lispy-" + path)
    if contents is None:
        contents = open(path, 'r').read()
        contents = (header % title) + contents.replace('<references />', code_html(codes)) + footer
        cache.set("lispy-" + path, contents, timeout=5*60)

    return contents

@app.route('/')
def route_index():
    return route_page('splash')

@app.errorhandler(404)
def route_404(e):
    return redirect(url_for('route_page', page='404'))

@app.route('/download/<id>/<type>')
@app.route('/download/<id>/BuildYourOwnLisp.<type>')
def route_download(id, type):

    keys = os.path.join(os.path.split(__file__)[0], 'purchases')

    with open(keys, 'r') as keyfile:
        keys = map(lambda x: x.strip(), keyfile.readlines())
        keys = map(lambda x: x.split(' '), keys)
        keys = set([key[1] for key in keys if
              (datetime.datetime.now() -
               datetime.datetime.strptime(key[0], '%Y-%m-%d-%H:%M:%S'))
             < datetime.timedelta(days=60)])

    if id in keys:
        if   type == 'epub': return send_file('BuildYourOwnLisp.epub',   mimetype='application/epub+zip')
        elif type == 'mobi': return send_file('BuildYourOwnLisp.mobi',   mimetype='application/x-mobipocket-ebook')
        elif type ==  'pdf': return send_file('BuildYourOwnLisp.pdf',    mimetype='application/pdf')
        elif type ==  'tar': return send_file('BuildYourOwnLisp.tar.gz', mimetype='application/x-gtar')
        else: return redirect(url_for('route_page', page='invalid'))

    else: return redirect(url_for('route_page', page='invalid'))


""" Paypal Stuff """

def ordered_storage(f):
    def decorator(*args, **kwargs):
        request.parameter_storage_class = ImmutableOrderedMultiDict
        return f(*args, **kwargs)
    return decorator
""" Main """

if __name__ == '__main__':
    port = int(os.getenv('PORT') or 5000)
    app.run(port=port, debug=True)

