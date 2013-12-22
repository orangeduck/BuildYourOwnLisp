from flask import Flask

header = """
<!DOCTYPE html>
<html>
  <head>
    <title>Build your own Lisp</title>
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <!-- Bootstrap -->
    <link href="static/css/bootstrap.css" rel="stylesheet">
    <link href="static/css/github.css" rel="stylesheet">

    <!-- HTML5 Shim and Respond.js IE8 support of HTML5 elements and media queries -->
    <!-- WARNING: Respond.js doesn't work if you view the page via file:// -->
    <!--[if lt IE 9]>
      <script src="https://oss.maxcdn.com/libs/html5shiv/3.7.0/html5shiv.js"></script>
      <script src="https://oss.maxcdn.com/libs/respond.js/1.3.0/respond.min.js"></script>
    <![endif]-->
  </head>
  <body style="background: url(static/img/tiletop.png) repeat-x;">
  
    <div style='margin:100px;width:800px;margin-left:auto;margin-right:auto;'>
    
"""

footer = """
  
    </div>
  
    <!-- jQuery (necessary for Bootstrap's JavaScript plugins) -->
    <script src="https://code.jquery.com/jquery.js"></script>
    <!-- Include all compiled plugins (below), or include individual files as needed -->
    <script src="static/js/bootstrap.min.js"></script>

    <!-- Syntax Highlighting -->
    <script src="static/js/rainbow.min.js"></script>
  </body>
</html>
"""

pages = [
    'chapter1_introduction.html',       'chapter2_the_basics.html',        'chapter3_crash_course.html',
    'chapter4_interactive_prompt.html', 'chapter5_introducing_mpc.html',   'chapter6_parsing.html',
    'chapter7_evaluation.html',         'chapter8_error_handling.html',    'chapter9_into_lisp.html', 
    'chapter10_variables.html',         'chapter11_functions.html',        'chapter12_conditionals.html',
    'chapter13_strings.html',           'chapter14_standard_library.html', 'chapter15_future_work.html'
]

app = Flask(__name__)

@app.route('/<page>')
def route_page(page):
    page = page + '.html'
    if not page in pages: return '404!'
    contents = open(page, 'r').read()
    contents = header + contents + footer
    return contents
    
@app.route('/')
def route_index():
    return route_page('chapter1_introduction')
    
if __name__ == '__main__':
    app.run()