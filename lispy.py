import os

from werkzeug.contrib.cache import MemcachedCache

from flask import Flask
from flask.ext.basicauth import BasicAuth

pages = [
    'splash.html', 'contents.html', 'credits.html', 'faq.html', '404.html',
    'chapter1_introduction.html',       'chapter2_installation.html',   'chapter3_basics.html',
    'chapter4_interactive_prompt.html', 'chapter5_languages.html',      'chapter6_parsing.html',
    'chapter7_evaluation.html',         'chapter8_error_handling.html', 'chapter9_s_expressions.html', 
    'chapter10_q_expressions.html',     'chapter11_variables.html',     'chapter12_functions.html',        
    'chapter13_conditionals.html',      'chapter14_strings.html',       'chapter15_standard_library.html',
    'chapter16_bonus_projects.html'
]

titles = [
    '', 'Contents', 'Credits', 'Frequently Asked Questions', 'Page Missing',
    'Introduction &bull; Chapter 1',       'Installation &bull; Chapter 2',   'Basics &bull; Chapter 3',
    'Interactive Prompt &bull; Chapter 4', 'languages &bull; Chapter 5',      'Parsing &bull; Chapter 6',
    'Evaluation &bull; Chapter 7',         'Error Handling &bull; Chapter 8', 'S-Expressions &bull; Chapter 9',
    'Q-Expressions &bull; Chapter 10',     'Variables &bull; Chapter 11',     'Functions &bull; Chapter 12',
    'Conditionals &bull; Chapter 13',      'Strings &bull; Chapter 14',       'Standard Library &bull; Chapter 15',
    'Bonus Projects &bull; Chapter 16'
]

header = """
<!DOCTYPE html>
<html>
  <head>
    <title>%s &bull; Build Your Own Lisp</title>
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <!-- Bootstrap -->
    <link href="static/css/bootstrap.css" rel="stylesheet">
    <link href="static/css/code.css" rel="stylesheet">
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
  
  </head>
  <body style="background: url(static/img/tiletop.png) repeat-x;">
  
    <div class='container' style='max-width:750px; margin-top:50px;'>
        <div class='row'>
         <div class='col-xs-12'>
    
"""

footer = """
         </div>
        </div> 
    </div>
  
    <!-- jQuery (necessary for Bootstrap's JavaScript plugins) -->
    <script src="https://code.jquery.com/jquery.js"></script>
    <!-- Include all compiled plugins (below), or include individual files as needed -->
    <script src="static/js/bootstrap.js"></script>

    <!-- Syntax Highlighting -->
    <script src="static/js/rainbow.js"></script>
    <script src="static/js/language/generic.js"></script>
    <script src="static/js/language/c.js"></script>
    <script src="static/js/language/lispy.js"></script>
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

app = Flask(__name__)
#app.config['BASIC_AUTH_USERNAME'] = 'byol'
#app.config['BASIC_AUTH_PASSWORD'] = 'lovelace'
#app.config['BASIC_AUTH_FORCE'] = True

#app_auth = BasicAuth(app)

@app.route('/<page>')
def route_page(page):
    page = page + '.html'
    if not page in pages: page = '404.html'
    path = os.path.join(os.path.split(__file__)[0], page)
    
    title = titles[pages.index(page)]
    
    contents = cache.get("lispy-" + path)
    if contents is None:
        contents = open(path, 'r').read()
        contents = (header % title) + contents + footer
        cache.set("lispy-" + path, contents, timeout=5*60)
        
    return contents
    
@app.route('/')
def route_index():
    return route_page('splash')
    
if __name__ == '__main__':
    app.run()
