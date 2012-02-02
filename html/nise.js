/*
var count = 0;
var history;
*/

var THUMBNAIL_SIZE = 100;

var result = {
    query_id: '',
    num_results: 0,
    more_page: 0,
    results_per_page: 0
};

function ShortenURL (url) {
    var domain = url.match(/:\/\/(.[^/]+)/)[1];
    var split = domain.split('.');
    if (split.length < 2) return domain;
    return split[split.length-2] + '.' + split[split.length-1];
}

function FormatResult (v) {
    var txt = '<span class="result"><span class="thumblink"><input type="hidden" value="' + v + '"/><img class="thumb" src="/thumb?id='
            + v + '"/></span>';
    /*
    txt += '<div class="source">';
    for (s in v.link) {
        txt += '<a href="' + v.link[s] + '">' + ShortenURL(v.link[s]) + '</a><br/>';
    }
    txt += '</div>*/
    txt += '</span>';
    return txt;
}

var JcropAPI = null;

var selection = {
    width: 1,
    height: 1,
    left: 0,
    pot: 0,
    right: 1.0,
    bottom: 1.0,
    crop: 1
};

function ResetSelection () {
    selection.width = 1;
    selection.height = 1;
    selection.left = 0;
    selection.right = 1.0;
    selection.pot = 0;
    selection.bottom = 1.0;
    selection.crop = 1;
}

var QueryThumb = null;

function ShowPage (page) {
    var text = '';
    for (i in page) {
        text += FormatResult(page[i]);
    }
    $('#results').html(text);
//    $("a.thumblink").click(QueryThumb);
}

function FormatPageLink (page, cur, text) {
    if (page == cur) {
        return '<span class="pagelink_dead">' + text + '</span>';
    }
    else {
        return '<span class="pagelink"><input type="hidden" value="' + page + '"/>' + text + '</span>';
    }
}

var GotoPage = null;

function ShowIndex (v) {
    if (v.page_count > 0) {
        $('#viewing').html(' Viewing images ' + (v.page_offset + 1)+ ' to ' + (v.page_offset + v.page_count) + '.');
    }
    else {
        $('#viewing').html('');
    }
    var num_page = Math.floor((result.num_results + result.results_per_page - 1)
            / result.results_per_page);
    var cur_page = Math.floor(v.page_offset / result.results_per_page);
    var txt = '';
    if (num_page > 1) {
        if (cur_page > 0) {
            txt += FormatPageLink(cur_page - 1, cur_page, 'Previous');
        }
        txt += FormatPageLink(0, cur_page, 1);
        var start = 1;
        if (start < cur_page - 2) {
            txt += FormatPageLink(cur_page, cur_page, '...');
            start = cur_page - 2;
        }
        var stop = num_page - 2;
        var dots = '';
        if (stop > cur_page + 2) {
            stop = cur_page + 2;
            dots = FormatPageLink(cur_page, cur_page, '...');
        }
        for (i = start; i <= stop; i++) {
            txt += FormatPageLink(i, cur_page, i + 1);
        }
        txt += dots;
        txt += FormatPageLink(num_page - 1, cur_page, num_page);
        if (cur_page < num_page - 1) {
            txt += FormatPageLink(cur_page + 1, cur_page, 'Next');
        }
    }
    if (!v.done) {
        txt += FormatPageLink(result.more_page, cur_page, 'More...');
    }
    $('#index').html(txt);
    /*
    $("a.pagelink").click(function (e) {
            e.preventDefault();
            GotoPage($(this).attr('href'));
            });
            */
}

function JcropDetach () {
    if (JcropAPI != null) JcropAPI.destroy();
}
function JcropAttach (w, h) {
    var tw = w;
    var th = h;
    if (tw > th) {
        th *= THUMBNAIL_SIZE / tw;
        tw = THUMBNAIL_SIZE;
    }
    else {
        tw *= THUMBNAIL_SIZE / th;
        th = THUMBNAIL_SIZE;
    }
    selection.width = tw;
    selection.height = th;

    JcropAPI = $.Jcrop('#crop', {width: tw, height: th});
}

function loading () {
    $('#viewing').html('');
    $('#total').html('');
    $('#results').html('<div id="loading"><img src="loading.gif"/></div>');
    $('#index').html('');
}

function SearchCallback (data) {
    $('#current_session').val('');
    $('#current_sha1').val('');
    if (data.expired != null) {
        result.query_id = '';
        result.num_results = 0;
        result.more_page = 0;
        $('#viewing').html('');
        $('#total').html('');
        $('#result').html('Your session has expired.');
        $('#index').html('');
        return;
    }
    else if (data.bad != null) {
        result.query_id = '';
        result.num_results = 0;
        result.more_page = 0;
        $('#viewing').html('');
        $('total').html('');
        $('result').html('An error has happened. Try retrieve your session from the search history.');
        $('index').html('');
        return;
    }

    $('#current_session').val(data.id);
    result.query_id = data.id;
    result.num_results = data.num_results;
    result.more_page = Math.floor(result.num_results / result.results_per_page);
    if (data.thumb) {
        JcropDetach();
        $('#crop').attr('src', "/search/thumb?id=" + result.query_id);
        JcropAttach(parseInt(data['image.width']), parseInt(data['image.height']));
        $('#query').show();
        $('#current_sha1').val(data.sha1);
    }
    else {
        $("#query").hide();
    }
    if (data.tiny != null) {
        var html = '<span class="historylink"><input type="hidden" value="'
                + data.id + '"/><img class="tiny" src="' + data.tiny
                +'"/></span>';
        $('#history_list span').each(function (idx) {
                var id = $(this).children("input").val();
                if (id != data.id) {
                    html += '<span class="historylink">' + $(this).html()
                            + '</span>';
                }
            });
        $('#history_list').html(html);
        /*
        $("a.historylink").click(function (e) {
            e.preventDefault();
            result.query_id = $(this).attr('href');
            ResetSelection();
            GotoPage(0);
            });
            */
    }

    {
        var txt = result.num_results + ' images found.'; // in ' + data.time + ' seconds.';
        if (!data.done) {
            txt += FormatPageLink(result.more_page, -1, 'Find more...');
        }
        $('#total').html(txt);
    }
    ShowIndex(data);
    ShowPage(data.page);
}

function MetaCallback (data) {
}

GotoPage = function (page) {
    var dt = new Date();
    // if query is the same, IE won't reload. so I add time to 
    // make every query different
    loading();
    $.ajax({
        type: "GET",
        url: "/search/page",
        async: true,
        data: {'id': result.query_id,
               'time_limit': $('#time_limit').val(),
               'page_offset': page * result.results_per_page,
               'page_count': result.results_per_page,
               'cur_time': dt.getTime()
              },
        dataType: 'json',
        success: SearchCallback
    });
}

QueryThumb = function (e) {
    e.preventDefault();
    //ResetSelection();
    $.ajax({
        type: "GET",
        url: "/search/local",
        data: {'id': $(this).children("input").val(),
               //'threshold': $('#threshold').val(),
               'expansion': $('#result_expansion').val(),
               'batch': $('#result_batch').val(),
               'time_limit': $('#time_limit').val(),
               'size': $('#size_select').val(),
               'page_offset': 0,
               'page_count': result.results_per_page
              },
        dataType: 'json',
        success: SearchCallback
    });
}

function SearchURL (e) {
    e.preventDefault();
    var url = $('#query_url').val();
    if (url.length < 5) {
        alert("Please input a proper URL address.");
        return;
    }
    loading();
    //ResetSelection();
    $.ajax({
        type: "GET",
        async: true,
        url: "/search/url",
        data: {'url': url,
               'threshold': $('#threshold').val(),
               'expansion': $('#result_expansion').val(),
               'batch': $('#result_batch').val(),
               'time_limit': $('#time_limit').val(),
               'size': $('#size_select').val(),
               'page_offset': 0,
               'page_count': result.results_per_page
              },
        dataType: 'json',
        success: SearchCallback
    });
}

function SearchSHA1 (e) {
    e.preventDefault();
    var sha1 = $('#query_sha1').val();
    if (sha1.length != 40) {
        alert("Please input a proper sha1 checksum.");
        return;
    }
    loading();
    //ResetSelection();
    $.ajax({
        type: "GET",
        async: true,
        url: "/search/sha1",
        data: {'sha1': sha1,
               'threshold': $('#threshold').val(),
               'expansion': $('#result_expansion').val(),
               'batch': $('#result_batch').val(),
               'time_limit': $('#time_limit').val(),
               'size': $('#size_select').val(),
               'page_offset': 0,
               'page_count': result.results_per_page
              },
        dataType: 'json',
        success: SearchCallback
    });
}

function SearchRandom (e) {
    e.preventDefault();
    loading();
    //ResetSelection();
    var dt = new Date();
    $.ajax({
        type: "GET",
        async: true,
        url: "/search/random",
        data: {'threshold': $('#threshold').val(),
               'expansion': $('#result_expansion').val(),
               'batch': $('#result_batch').val(),
               'time_limit': $('#time_limit').val(),
               'size': $('#size_select').val(),
               'page_offset': 0,
               'page_count': result.results_per_page,
               'cur_time': dt.getTime()
             },
        dataType: 'json',
        success: SearchCallback
    });
}

function SearchDemo (demo) {
    loading();
    //ResetSelection();
    $.ajax({
        type: "GET",
        async: true,
        url: "/search/demo",
        data: {'id': demo,
               'threshold': $('#threshold').val(),
               'expansion': $('#result_expansion').val(),
               'batch': $('#result_batch').val(),
               'time_limit': $('#time_limit').val(),
               'size': $('#size_select').val(),
               'page_offset': 0,
               'page_count': result.results_per_page
             },
        dataType: 'json',
        success: SearchCallback
    });
}

function DemoCallback (d) {
    $('#viewing').html('Click one of the images to search...');
    $('#total').html('');
    $('#index').html('');
    var text = '';
    for (i in d.page) {
        var img = d.page[i];
        text += 
        '<span class="demolink"><input type="hidden" value="' + img + '"/><img class="demo" src="/demo/image?id=' + img + '"/></span>';
    }
    $('#results').html(text);
}

function ShowDemo (e) {
    e.preventDefault();
    loading();
    //ResetSelection();
    var dt = new Date();
    $.ajax({
        type: "GET",
        async: true,
        data: {
            'cur_time': dt.getTime()
        },
        url: "/demo/list",
        dataType: 'json',
        success: DemoCallback
    });
}

function Resubmit () {
    if (result.query_id == null) return;
    if (result.query_id.length < 1) return;
    loading();
    $.ajax({
        type: "GET",
        url: "/search/update",
        data: {'id': result.query_id,
               'threshold': $('#threshold').val(),
               'expansion': $('#result_expansion').val(),
               'batch': $('#result_batch').val(),
               'size': $('#size_select').val(),
               'page_offset': 0,
               'page_count': result.results_per_page,
               'box.left': selection.left,
               'box.right': selection.right,
               'box.top': selection.pot,
               'box.bottom': selection.bottom,
               'crop': selection.crop
             },
        dataType: 'json',
        success: SearchCallback
    });
}

function Refresh () {
    result.results_per_page = parseInt($('#results_per_page').val());
    if (result.query_id != null && result.num_results > 0) {
        GotoPage(0);
    }
}

function Crop (c, cr) {
    selection.left = 1.0 * c.x / selection.width;
    selection.pot =  1.0 * c.y / selection.height;
    selection.right = 1.0 * c.x2 / selection.width;
    selection.bottom = 1.0 * c.y2 / selection.height;
    selection.crop = cr;
    Resubmit();
    ResetSelection();
}

$(document).ready(function() {

    $('#docrop').click(function () {
        Crop(JcropAPI.tellSelect(), 1);
    });
    $('#docut').click(function () {
        Crop(JcropAPI.tellSelect(), 0);
    });
    $('#options_toggle').click(function () {
            $('#options').toggle("fast");
        });
    $('#advanced_toggle').click(function () {
            $('#advanced').toggle("fast");
        });
    $('#history_toggle').click(function () {
            $('#history').toggle("fast");
        });
    if ($.browser.msie) {
        $('#upload_form').ajaxForm({
            // why is IE that stupid?
            dataType: 'text',
            success: function(txt) {
                var p1 = txt.indexOf('{');
                var p2 = txt.lastIndexOf('}');
                txt = txt.substr(p1, p2 - p1 + 1);
                var data = eval('(' + txt + ')');
                SearchCallback(data);
            }
        });
    }
    else {
        $('#upload_form').ajaxForm({
            dataType: 'json',
            success: SearchCallback
        });
    }
    $('#upload_form').submit(function () {
        loading();
        //ResetSelection();
        $('#form_threshold').val($('#threshold').val());
        $('#form_expansion').val($('#result_expansion').val());
        $('#form_batch').val($('#result_batch').val());
        $('#form_size').val($('#size_select').val());
        $('#form_offset').val(0);
        $('#form_count').val($('#results_per_page').val());
        $('#form_time').val($('#time_limit').val());
        $(this).ajaxSubmit();
        return false;
    });
    $('#search_url').click(SearchURL);
    $('#search_random').click(SearchRandom);
    $('#search_sha1').click(SearchSHA1);
    $('#recover_session').click(function (e) {
                e.preventDefault();
                var k = $("#session_key").val();
                if (k.length != 36) {
                    alert("Invalid session id");
                    return;
                }
                result.query_id = k;
                GotoPage(0);
            });
    $('#show_demo').click(ShowDemo);
    $('.resubmit').change(Resubmit);
    $('.refresh').change(Refresh);
    $('#clear_history').click(function (e) {
            e.preventDefault();
            $('#history_list').html("");
    });

    // Delegate doesn't work with IE : (
    $("body").delegate("span.thumblink", 'click',  QueryThumb);
    /*
    $("body").delegate("a.thumblink", 'hover',  function (e) {
            }, function (e) {
            });
            */
    $("body").delegate("span.pagelink", "click", function (e) {
            GotoPage($(this).children("input").val());
            });
    $("body").delegate("span.demolink", "click", function (e) {
            SearchDemo($(this).children("input").val());
            });
    $("body").delegate("span.historylink", "click", function (e) {
            result.query_id = $(this).children("input").val();
            //ResetSelection();
            GotoPage(0);
            });

    result.results_per_page = parseInt($('#results_per_page').val());

//    SearchRandom();
});

