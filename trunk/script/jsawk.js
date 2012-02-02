#!/bin/env js 

options('strict');

function array2json(arr) {

    var parts = [];
    var is_list = (Object.prototype.toString.apply(arr) === '[object Array]');

    if ((typeof arr == "number") || (typeof arr == "string")) {
        return arr;
    }

    for(var key in arr) {
        var value = arr[key];
        if(typeof value == "object") { //Custom handling for arrays
            if(is_list) parts.push(array2json(value)); /* :RECURSION: */
            else parts[key] = array2json(value); /* :RECURSION: */
        }
        else {
            var str = "";
            if(!is_list) str = '"' + key + '":';

            //Custom handling for multiple data types
            if(typeof value == "number") str += value; //Numbers
            else if(value === false) str += 'false'; //The booleans
            else if(value === true) str += 'true';
            else str += '"' + value + '"'; //All other things
            // :TODO: Is there any more datatype we should be in the lookout for? (Functions?)

            parts.push(str);
        }
    }
    var json = parts.join(",");
    
    if(is_list) return '[' + json + ']';//Return numerical JSON
    return '{' + json + '}';//Return associative JSON
}

function test (e) {
    return true;
}

function extract (e) {
    return e;
}

if (arguments.length >= 1) {
    var code = arguments[0];
    if (code.length >= 2) {
        var sep = code.charAt(0);
        var second = code.indexOf(sep, 1);
        if (second == -1) {
            test = eval('function ($) { return ' + code.substr(1, code.length -1) + ';}'); 
        }
        else {
            test = eval('function ($) { return ' + code.substr(1, second - 1) + ';}'); 
            extract = eval('function ($) { return ' + code.substr(second + 1, code.length - second - 1) + ';}');
        }
    }
}

for (;;) {
    var line = readline();
    if (line == null) break;
    line = line.trim();
    if (line.length == 0) continue;
    obj = eval('(' + line + ')');
    if (test(obj)) {
        print(array2json(extract(obj)));
    }
}

