import os
import logging

from werkzeug.contrib.cache import MemcachedCache
from werkzeug.datastructures import ImmutableOrderedMultiDict

from flask import Flask, jsonify, request
from flask.ext.mail import Mail, Message
from urllib2 import urlopen

pages = [
    'splash.html', 'contents.html', 'credits.html', 'faq.html', '404.html', 'ebook.html', 'test.html',
    'chapter1_introduction.html',       'chapter2_installation.html',   'chapter3_basics.html',
    'chapter4_interactive_prompt.html', 'chapter5_languages.html',      'chapter6_parsing.html',
    'chapter7_evaluation.html',         'chapter8_error_handling.html', 'chapter9_s_expressions.html', 
    'chapter10_q_expressions.html',     'chapter11_variables.html',     'chapter12_functions.html',        
    'chapter13_conditionals.html',      'chapter14_strings.html',       'chapter15_standard_library.html',
    'chapter16_bonus_projects.html'
]

titles = [
    '', 'Contents', 'Credits', 'Frequently Asked Questions', 'Page Missing', 'eBook', 'Test',
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
  <style>
  @media (max-width: 640px) {
      h4 { font-size: 90%%; }
      }
  </style>
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

app  = Flask(__name__)
mail = Mail(app)

handler = logging.FileHandler(os.path.join(os.path.split(__file__)[0], 'error.log'))
handler.setLevel(logging.INFO)
app.logger.addHandler(handler)

#from flask.ext.basicauth import BasicAuth
#app.config['BASIC_AUTH_USERNAME'] = 'byol'
#app.config['BASIC_AUTH_PASSWORD'] = 'lovelace'
#app.config['BASIC_AUTH_FORCE'] = True
#app_auth = BasicAuth(app)

""" Page """

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
    
""" Paypal Stuff """

def ordered_storage(f):
    def decorator(*args, **kwargs):
        request.parameter_storage_class = ImmutableOrderedMultiDict
        return f(*args, **kwargs)
    return decorator

@app.route('/paypal', methods=['POST'])
@ordered_storage
def route_paypal():
    
    verify_string = '&'.join(('%s=%s' % (param, value) for param, value in request.form.iteritems()))
    verify_string = verify_string + '&%s=%s' % ('cmd', '_notify-validate')
    
    status = urlopen('https://www.paypal.com/cgi-bin/webscr', data=verify_string).read()
    
    if status == 'VERIFIED':
        
        msg = Message("Build Your Own Lisp - eBook Attached",
            sender="contact@buildyourownlisp.com",
            recipients=[request.form.get('payer_email')],
            
            body="Hello,\n"
                 "\n"
                 "Many thanks for purchasing the eBook for Build Your Own Lisp. "
                 "I really appreciate your contribution and support!\n"
                 "\n"
                 "Attached is the eBook in .pub, .mobi, and .pdf format. "
                 "If you need it in a different format, or need any help "
                 "using these files don't hesitate to get in contact. "
                 "I will do what I can to help. I can also provide these "
                 "files for you in a different way if you are having "
                 "trouble accessing the attachments. \n"
                 "\n"
                 "This e-mail should be considered a proof of purchase. "
                 "If you want an updated version of the eBook please "
                 "contact this address, with a copy of this e-mail. "
                 "I will supply you with an updated copy. "
                 "If you have any other problems or questions, please "
                 "contact this address and I will try to resolve them "
                 "as soon as possible.\n"
                 "\n"
                 "Thanks again, and I hope you enjoy the book!\n"
                 "\n"
                 "- Dan\n")
        
        ebook_files = ["BuildYourOwnLisp.epub", "BuildYourOwnLisp.mobi", "BuildYourOwnLisp.pdf"]
        ebook_mimes = ["application/epub+zip", "application/x-mobipocket-ebook", "application/pdf"]
        
        for file, mime in zip(ebook_files, ebook_mimes):
            with app.open_resource(file) as f: msg.attach(file, mime, f.read())
        
        mail.send(msg)
        
    else:
        app.logger.error('Paypal IPN string %s did not validate' % verify_string)

    return jsonify({'status': 'complete'})
    
""" Main """
    
if __name__ == '__main__':
    app.run()

