/**
 * Lispy patterns
 *
 * @author Daniel Holden
 * @version 1.0
 */
Rainbow.extend('lispy', [
    {
        'name': 'keyword.operator',
        'pattern': /\(|\)|\{|\}|\&amp\;|\+|\-|\*|\/|\\|\=|\!|\&gt\;|\&lt\;/g
    },
    {
        'name': 'keyword.builtin',
        'pattern': /\b(def|list|head|tail|eval|join|if|load|error|print)\b/g
    },
    {
        'name': 'keyword.library',
        'pattern': /\b(fun|unpack|pack|curry|uncurry|do|let|not|or|and|flip|ghost|comp|fst|snd|trd|len|nth|last|take|drop|while|split|elem|map|filter|foldl|foldr|sum|product|select|otherwise|case|fib|true|false|nil|lookup|zip|unzip|reverse)\b/g
    },
    {
        'name': 'constant.numeric',
        'pattern': /-?[0-9]+/g
    },
    {
        'name': 'constant.string',
        'pattern': /\"(\\.|[^\"])*\"/g
    },
    {
        'name': 'meta.prompt',
        'pattern': /lispy\&gt\;/g
    },
    {
        'name': 'meta.error',
        'pattern': /Error\: [^\r\n]*/g
    },
    {
        'name': 'meta.error',
        'pattern': /\&lt\;stdin\&gt\;[^\r\n]*/g
    },
    {
        'name': 'meta.version',
        'pattern': /Lispy Version[^\r\n]*/g
    },
    {
        'name': 'meta.exit',
        'pattern': /Press Ctrl\+c[^\r\n]*/g
    },
    {
        'name': 'comment',
        'pattern': /;[^\r\n]*/g
    }
], true);
