var activewidget = null;
var tracks_by_path = {};
var library = null;
var playlists = null;
var selected_playlist = null;

var lines = 'yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>yo<br/>'

//
//var playlists;
//var currently_selected_playlist = -1;
//
//function populatePlaylists() {
//	var tbody = jQuery("#playlists tbody");
//	var j = 0;
//
//	console.log("updating with playlist ver " + playlists.version);
//	tbody.empty();
//	$.each( playlists.playlists, function( i, playlist ) {
//		var prefix = "";
//		if( i == currently_selected_playlist ) { prefix = "&gt;"; }
//		var x = $("<tr><td class=\"icon\">" + prefix + "</td><td><a href=\"javascript:selectPlaylist(" + i + ");\">" + playlist.name + "</a></td></tr>");
//		tbody.append( x );
//	});
//}
//
//function populatePlaylist() {
//	var tbody = jQuery("#playlist tbody");
//	var j = 0;
//
//	tbody.empty();
//	$.each( playlists.playlists[currently_selected_playlist].items, function( i, item ) {
//		var prefix = "";
//		var x = $("<tr><td class=\"icon\">" + prefix + "</td><td><a href=\"javascript:play(" + currently_selected_playlist + ", " + i + ");\">" + item.path + "</td></tr>");
//		tbody.append( x );
//	});
//}
//
//function refreshPlaylists() {
//	$.getJSON("/playlists")
//		.then(function(data) {
//			playlists = data;
//			populatePlaylists();
//			populatePlaylist();
//		});
//}
//
//function selectPlaylist(id) {
//	currently_selected_playlist = id;
//	populatePlaylist();
//}
//
//function play(playlist_id, track_id) {
//	$.post( "/play?playlist=" + playlist_id + "&track=" + track_id, function( data ) { });
//}
//
//
////var albums, playlists;
////$.when(
////    $.getJSON("/albums", function(data) {
////        albums = data;
////    }),
////    $.getJSON("/playlists", function(data) {
////        playlists = data;
////    })
////).then(function() {
////});
////
////
////var albums = [];
////$.getJSON( "/albums", function( data ) {
////	albums = data.albums;
////	$.each( albums, function( i, album ) {
////		album.id = i;
////	});
////	populateTable("");
////});
////
////var fuse_options = {
////	shouldSort: true,
////	threshold: 0.6,
////	location: 0,
////	distance: 100,
////	maxPatternLength: 32,
////	minMatchCharLength: 1,
////	keys: [
////		"artist",
////		"name"
////	]
////};
////
////var matches = [];
////function populateTable(filter) {
////	var tbody = jQuery("#albums tbody");
////	var j = 0;
////	matches = [];
////	if( filter == "" ) {
////		matches = albums;
////	} else {
////		var fuse = new Fuse(albums, fuse_options);
////		matches = fuse.search(filter);
////	}
////
////	if( current_selection >= matches.length ) {
////		current_selection = matches.length - 1;
////	}
////
////	tbody.empty();
////	$.each( matches, function( i, album ) {
////		var prefix = "";
////		if( i == current_selection ) {
////			prefix = "&gt; ";
////		}
////		var x = $("<tr><td class=\"icon\">" + prefix + "</td><td>" + album.artist + "</td><td>" + album.name + "</td></tr>");
////		tbody.append( x );
////	});
////}
////
////var current_selection = 0;
////$("#filter").keydown(function(e) {
////	switch(e.which) {
////		case 38: // up
////			current_selection = Math.max( current_selection-1, 0 );
////			break;
////
////		case 40: // down
////			current_selection++;
////			break;
////
////		case 13: // down
////			console.log("play song");
////			var album = matches[current_selection];
////			$.post( "/albums?album=" + album.id, function( data ) { });
////
////			break;
////
////		default:
////
////			current_selection = 0;
////			return; // exit this handler for other keys
////	}
////	e.preventDefault(); // prevent the default action (scroll / move caret)
////});
////
////
////$("#filter").keyup(function() {
////	populateTable(this.value);
////});
////
////$("#filter").attr("autocomplete", "off");
////$("#filter").select();
////
////function updateCurrentSong(msg) {
////	$("#currentsong").text(msg);
////}
////
////updateCurrentSong("nothing");
////
//
//var first_msg = true;
//var last_msg;
//function updatePlaying(msg) {
//	last_msg = msg;
//	if( !playlists || msg.playlist_version != playlists.version ) {
//		if( first_msg ) {
//			first_msg = false;
//			currently_selected_playlist = msg.playlist_id;
//		}
//		refreshPlaylists();
//	}
//}
//
//function createWebsocket() {
//	var exampleSocket = new WebSocket("ws://" + document.domain + ':' + location.port + "/websocket");
//	exampleSocket.onopen = function (event) {
//		console.log("websocket opened");
//	}
//	exampleSocket.onmessage = function (event) {
//		console.log("got message: \"" + event.data + "\"");
//		if( event.data == "" ) {
//			return;
//		}
//		var msg = JSON.parse(event.data);
//		updatePlaying(msg);
//		//updateCurrentSong(msg.artist + " - " + msg.title);
//	}
//}
//
//$(document).ready(function() {
//	createWebsocket();
//
//});
//
//
//

function td(text) {
  var td = $('<td>');
  td.text(text);
  return td;
}

function buildtable(headers, data) {
  var n = data.length;
  var m = headers.length;

  var header = ['<table class="myheader pure-table">'];
  header.push('<thead><tr>');
  for( var i = 0; i < m; i++ ) {
    header.push('<th>' + headers[i] + '</th>');
  }
  header.push('</tr></thead></table>');

  var body = ['<table class="mybody pure-table"><tbody>'];
  

  for( var i = 0; i < n; i++ ) {
    body.push('<tr>');
    for( var j = 0; j < m; j++ ) {
      body.push('<td>' + data[i][j] + '</td>');
    }
    body.push('</tr>');
  }
  body.push('</tbody></table>');

  return {
    'header': $(header.join('')),
    'body': $(body.join(''))
  };
}

function load() {
  tracks_by_path = {};
  $.each(library.albums, function(i, album) {
    $.each(album.tracks, function(i, track) {
      tracks_by_path[track.path] = {
        "track": track,
        "album": album
      };
    });
  });

  console.log('finding default');
  $.each(playlists.playlists, function(i, playlist) {
    if( playlist.name == "default" ) {
      playlistwidget.load(playlist);
    }
  });

  selectorwidget.load(library.albums);

  activewidget = playlistwidget;
  activewidget.refresh();
  
  //  //console.log('start load2');
  //  //});
  //  //$('#library').empty().append(buildtable(['artist', 'album', 'year', 'title', 'length'], rows));
  //
  //  var selected_playlist = null;
  //  $.each(playlists.playlists, function(i, playlist) {
  //    if( playlist.name == "default" ) {
  //      selected_playlist = playlist;
  //    }
  //  });
  //
  //  var rows = [];
  //  $.each(selected_playlist.items, function(i, item) {
  //    var track = tracks[item.path];
  //    console.log(track.album);
  //    var cols = [];
  //    cols.push(track.album.artist);
  //    cols.push(track.album.album);
  //    cols.push(track.album.year);
  //    cols.push(track.track.title);
  //    cols.push(track.track.length);
  //    rows.push(cols)
  //  });
  //  $('#playlist').empty().append(buildtable(['artist', 'album', 'title', 'length'], rows));
  //
  //
  //  console.log('end load2');
  //  console.log(rows.length);
}

function showAdd() {
  $('#playlist').empty().append($("<b>select</b>"));
}

function refresh(cb) {
  $.when(
    $.getJSON("/library", function(data) {
        library = data;
    }),
    $.getJSON("/playlists", function(data) {
        playlists = data;
    })
  ).then(function() {
    load();
    if( cb !== undefined ) { cb(); }
  });

}

function playSelectedPlaylistItem() {
  alert("todo play: " + selected_playlist_item);
}


var loadingwidget = {
  refresh: function() {
    console.log('loading refresh');
    $('#playlist').empty().append($("<b>loading...</b>"));
  },
  onkeydown: function (e) {
    return false;
  }
};

//var playlistwidget = {
//  refresh: function() {
//    console.log('playlist refresh');
//
//    var rows = [];
//    console.log('playlist refresh1');
//    $.each(selected_playlist.items, function(i, item) {
//      var track = tracks_by_path[item.path];
//      console.log(track.album);
//      var cols = [];
//      cols.push(track.album.artist);
//      cols.push(track.album.album);
//      cols.push(track.album.year);
//      cols.push(track.track.title);
//      cols.push(track.track.length);
//      rows.push(cols)
//    });
//    console.log('playlist refresh2');
//    $('#playlist').empty().append('<b>playlist</b>').append(buildtable(['artist', 'album', 'title', 'length'], rows));
//  },
//  onkeydown: function (e) {
//    switch (e.keyCode) {
//        case 37:
//            console.log('left');
//            break;
//        case 38:
//          // up
//          selectPlaylistItem(-1);
//          return false;
//        case 39:
//            alert('right');
//            break;
//        case 40:
//          // down
//          selectPlaylistItem(1);
//          return false;
//        case 13:
//          // enter
//          playSelectedPlaylistItem();
//          return false;
//        case 65:
//          // a (add)
//          activewidget = trackselectorwidget;
//          activewidget.refresh();
//          return false;
//    }
//    console.log(e.keyCode);
//  }
//};
//
//var trackselectorwidget = {
//  refresh: function() {
//    var rows = [];
//    $.each(tracks_by_path, function(path, track) {
//      var cols = [];
//      cols.push(track.album.artist);
//      cols.push(track.album.album);
//      cols.push(track.album.year);
//      cols.push(track.track.title);
//      cols.push(track.track.length);
//      rows.push(cols)
//    });
//    $('#playlist').empty().append('<b>selector</b>').append(buildtable(['artist', 'album', 'title', 'length'], rows));
//  },
//  onkeydown: function (e) {
//    switch (e.keyCode) {
//      case 27:
//        // escape
//        activewidget = playlistwidget;
//        activewidget.refresh();
//        return false;
//      case 13:
//        // enter
//        activewidget = playlistwidget;
//        activewidget.refresh();
//        return false;
//    }
//    console.log(e.keyCode);
//  }
//};

function trackTable() {
  this.name = 'tracktable';
  this.selected = 0;
  this.rows = [];
  this.headers = ['artist', 'album', 'title', 'length'];
  //this.header = null;
  //this.body = null;
};
trackTable.prototype.refresh = function() {
  var table = buildtable(this.headers, this.rows);

  var sticky = $('<div style="position: fixed; top: 0; width:100%">TODO: top part</div>');
  sticky.append(table.header);

  var body = $('<div></div>');
  body.append(table.body);

  $('#thepage').empty().append(sticky).append(body);

  body.css('margin-top', sticky.height());

  // TODO make the columns match the width
  //var foo = table.header.find("tr").first().children();
  //table.body.find("tr").first().children().each(function(i, td) {
  //  console.log(i);
  //  foo.eq(i).css('width', 10);
  //  console.log(td);
  //  console.log(td.width);
  //});
};
trackTable.prototype.onkeydown = function(e) {
  switch (e.keyCode) {
      case 37:
          console.log('left');
          break;
      case 38: // up
      case 75: // k
        this.selectPlaylistItem(-1);
        return false;
      case 39:
          alert('right');
          break;
      case 40: // down
      case 74: // j
        this.selectPlaylistItem(1);
        return false;
      case 13:
        // enter
        this.onenter();
        return false;
  }
  console.log(e.keyCode);
};

trackTable.prototype.selectPlaylistItem = function(x) {
  $('.mybody tr').eq(this.selected).removeClass("pure-table-odd");
  this.selected += x;
  if( this.selected < 0 ) {
    this.selected = 0;
  }
  if( this.selected >= this.rows.length ) {
    this.selected = this.rows.length - 1;
  }
  $('.mybody tr').eq(this.selected).addClass("pure-table-odd");
  var visible = this.selected-4;
  if( visible < 0 ) { visible = 0; }
  $.scrollTo($('.mybody tr').eq(visible).offset().top-50); // TODO 50 should be the height of the fixed header
}
trackTable.prototype.onenter = function() {
  console.log(this.selected);
}

function playlist() {
  trackTable.call(this);
  this.name = 'playlist';
  this.paths = [];
  this.ids = [];
  this.headers = ['artist', 'album', 'year', 'title', 'length', 'id'];
}
playlist.prototype = Object.create(trackTable.prototype);
playlist.prototype.onkeydown = function(e) {
  switch (e.keyCode) {
    case 65:
      // 'a' add
      activewidget = selectorwidget;
      selectorwidget.playlist = this;
      activewidget.refresh();
      return false;
  }
  return trackTable.prototype.onkeydown.call(this, e);
}
playlist.prototype.add_track = function(path, id) {
  var track = tracks_by_path[path];
  var cols = [];
  cols.push(track.album.artist);
  cols.push(track.album.album);
  cols.push(track.album.year);
  cols.push(track.track.title);
  cols.push(track.track.length);
  cols.push(id);
  this.rows.push(cols)
  this.paths.push(path);
  this.ids.push(id);
}
playlist.prototype.load = function(playlist) {
  this.paths = [];
  this.ids = [];
  this.rows = [];
  for( var i = 0; i < playlist.items.length; i++ ) {
    this.add_track(playlist.items[i].path, playlist.items[i].id);
  }
}
playlist.prototype.onenter = function() {
  var path = this.paths[this.selected];
  console.log("play " + path);

  var playlist_payload = [];
  for( var i = 0; i < this.paths.length; i++ ) {
    playlist_payload.push([this.paths[i], 0]);
  }

  var payload = {
    'name': 'default',
    'playlist': playlist_payload
  };

  var that = this;

  $.post( "/playlists", JSON.stringify(payload))
    .done(function( data ) {
      console.log('posted');
      console.log(data);
      refresh(function(){
        that.play_selected();
      });
    })
    .fail(function( data ) {
      console.log('failed to post playlist');
      console.log(data);
    });
}
playlist.prototype.play_selected = function() {
    console.log('here');
    var path = this.paths[this.selected];
    var id = this.ids[this.selected];
    $.post( "/play?playlist=default&track=" + this.selected + "&when=immediate")
      .done(function(data) {
        console.log('it worked');
        console.log(data);
      })
      .fail(function(data) {
        console.log('it failed');
        console.log(data);
      });
}

function selector() {
  trackTable.call(this);
  this.name = 'selector';
  this.playlist = null;
}
selector.prototype = Object.create(trackTable.prototype);
selector.prototype.load = function(albums) {
  var rows = [];
  var paths = []
  $.each(albums, function(i, album) {
    $.each(album.tracks, function(i, track) {
      var cols = [];
      cols.push(album.artist);
      cols.push(album.album);
      cols.push(album.year);
      cols.push(track.title);
      cols.push(track.length);
      rows.push(cols)
      paths.push(track.path);
    });
  });
  this.rows = rows;
  this.paths = paths;
  console.log(rows);
}
selector.prototype.onkeydown = function(e) {
  switch (e.keyCode) {
    case 27:
      // 'escape' escape
      activewidget = this.playlist;
      activewidget.refresh();
      return false;
  }
  return trackTable.prototype.onkeydown.call(this, e);
}
selector.prototype.onenter = function() {
  var path = this.paths[this.selected];
  console.log("adding " + path);
  this.playlist.add_track(path);
}

playlistwidget = new playlist();
selectorwidget = new selector();

function playmethehits() {
  activewidget = loadingwidget;
  activewidget.refresh();
  refresh();

  document.onkeydown = function(e) {
    return activewidget.onkeydown(e);
  };
}


$(window).on('load', function () {
    playmethehits();
});
