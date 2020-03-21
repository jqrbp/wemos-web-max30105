var head = 0, tail = 0, ring = new Array();
var myJSON;
var redval = 0;
var irval = 0;

function getIr() {
	return irval;
}  

function getRed() {
	return redval;
}

var cnt = 0;
setInterval(function(){
	Plotly.extendTraces('chart',{y: [[getIr()], [getRed()]]}, [0, 1]);
	cnt++;
	if(cnt > 500) {
		Plotly.relayout('chart',{
			xaxis: {
				range: [cnt-500,cnt]
			}
		});
	}
},15);

function get_appropriate_ws_url(extra_url)
{
	var pcol;
	var u = document.URL;

	/*
	 * We open the websocket encrypted if this page came on an
	 * https:// url itself, otherwise unencrypted
	 */

	if (u.substring(0, 5) === "https") {
		pcol = "wss://";
		u = u.substr(8);
	} else {
		pcol = "ws://";
		if (u.substring(0, 4) === "http")
			u = u.substr(7);
	}

	u = u.split("/");
	var p = u[0].split(":");

	/* + "/xxx" bit is for IE10 workaround */

	return pcol + p[0] + ":8181" + "/" + extra_url;
}

function new_ws(urlpath, protocol)
{
	if (typeof MozWebSocket != "undefined")
		return new MozWebSocket(urlpath, protocol);

	return new WebSocket(urlpath, protocol);
}

function isJSONHRValid(_str) {
	myJSON = null;
	try {
		myJSON = JSON.parse(_str);
	} catch (e) {
		return false;
	}

	if(myJSON.f == undefined) return false;
	if(myJSON.ir == undefined) return false;
	if(myJSON.red == undefined) return false;
	if(myJSON.bpm == null) return false;
	if(myJSON.ave == undefined) return false;
	if(myJSON.hr == undefined) return false;
	if(myJSON.spo == undefined) return false;

	return true;
}

document.addEventListener("DOMContentLoaded", function() {

	var ws = new_ws(get_appropriate_ws_url(""), ['arduino']);
	try {
		ws.onopen = function() {
			document.getElementById("r").disabled = 0;
		};
	
		ws.onmessage =function got_packet(msg) {
			var n, s = "";
			var msgdata = msg.data;
			var fcol = "0 0 0 gray";

			if(isJSONHRValid(msgdata)) {
				document.getElementById("ir").value = myJSON.ir; 
				document.getElementById("red").value = myJSON.red; 
				document.getElementById("bpm").value = myJSON.bpm; 
				document.getElementById("ave").value = myJSON.ave; 
				document.getElementById("hr").value = myJSON.hr;
				document.getElementById("spo").value = myJSON.spo; 
				irval = myJSON.ir;
				redval = myJSON.red;
				document.getElementById("dfast").innerHTML = myJSON.bpm; 
				document.getElementById("dave").innerHTML = myJSON.ave; 
				document.getElementById("dhr").innerHTML = myJSON.hr;
				document.getElementById("dspo").innerHTML = myJSON.spo;
				if(myJSON.f == 1) {
					fcol = "0 0 0 red";
				}
				document.getElementById("dvalid").style.textShadow = fcol;
			}

			ring[head] = msgdata + "\n";
			head = (head + 1) % 50;
			if (tail === head)
				tail = (tail + 1) % 50;
	
			n = tail;
			do {
				s = s + ring[n];
				n = (n + 1) % 50;
			} while (n !== head);
			
			document.getElementById("r").value = s; 
			document.getElementById("r").scrollTop =
			document.getElementById("r").scrollHeight;
		
		};
	
		ws.onclose = function(){
			document.getElementById("r").disabled = 1;
		};
	} catch(exception) {
		alert("<p>Error " + exception);  
	}

	Plotly.plot('chart',[{
		y:[getIr()],
		type:'line',
		name:'IR Val'
	},{
		y:[getRed()],
		type:'line',
		name:'Red Val'
	}]);
}, false);
