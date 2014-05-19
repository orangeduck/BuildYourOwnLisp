/**
 * C patterns
 *
 * @author Daniel Holden
 * @author Craig Campbell
 * @version 1.0.7
 */
Rainbow.extend('c', [
    {
        'name': 'comment',
        'pattern': /\/\*.*?\*\/|\/\/[^\r\n]*/g
    },
    {
        'name': 'keyword',
        'pattern': /\b(for|while|do|goto|typedef|return|if|else|switch|case|break|continue|NULL|sizeof)\b/g
    },
    {
        'name': 'keyword.operator',
        'pattern': /\+|\!|\-|\&(gt|lt|amp)\;|\||\*|\=|\:|\;|\?|\[|\]|\(|\)|\{|\}/g
    },
    {
        'name': 'constant.numeric',
        'pattern': /\b(\d+(\.\d+)?(e(\+|\-)?\d+)?(f|d)?|0x[\da-f]+)\b/gi
    },
    {
        'name': 'constant.string',
        'pattern': /\"(\\.|[^\"])*\"/g
    },
    {
        'name': 'constant.character',
        'pattern': /\'(\\.|[^\'])\'/g
    },
    {
        'name': 'meta.preprocessor',
        'pattern': /\#(\\(\r)?\n|[^\r\n])*/g
    },
    {
        'name': 'support.type',
        'pattern': /\b((un)?signed|void|char|short|int|long|float|double)\b/g
    },
    {
        'name': 'storage.modifier',
        'pattern': /\b(const|static|extern|auto|register|volatile|inline)\b/g
    },
    {
        'name': 'storage.type',
        'pattern': /\b(struct|union|enum)\b/g
    }
], true);
