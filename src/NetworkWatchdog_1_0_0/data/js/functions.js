function waitResult(reqUrl, reqData){
	let xhttp = new XMLHttpRequest();
	let req = $.ajax({
	  url: reqUrl,
	  type: "get",
	  headers: {
		'Access-Control-Allow-Headers': '*',
		'Access-Control-Allow-Origin': '*',
		},
	  data: reqData,
	  success: function(response) {
		if(req.responseText=="wait"){
			console.log("waiting for result");
			setTimeout(waitResult, 1000, reqUrl,reqData);
		}else{
			if(req.responseText==="success"){
				showMsg("<strong>SUCCESS</strong><br/> IP/URL is reachable.");
			}else if(req.responseText==="failed"){
				showMsg("<strong>FAILED</strong><br/> No Reply from the specific IP/URL.");				
			}else{
				showMsg(req.responseText);				
			}
			return true;
		}
	  },
	  error: function(xhr) {
		showMsg("Unable to ping");
	  }
	});
}

function ping(val){
	if(isValidIP(val)){
		waitResult("/ping",{ip:val});
	}else if(isValidURL(val)){
		waitResult("/ping",{url:val});
	}else{
		hideSpinner();
		showMsg("Not valid IP/URL");		
		return false;
	}
}

function restartDevice() {
  let xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
	if (this.readyState == 4 && this.status == 200) {
		showMsg("<strong>SUCCESS</strong><br/> Connected device is now restarting.");
	}
  };
	xhttp.open("GET", '/config?restart=true', true);
	xhttp.setRequestHeader('Access-Control-Allow-Headers', '*');
	xhttp.setRequestHeader('Access-Control-Allow-Origin', '*');
	xhttp.send();
}

function testConnection() {
  pingDst = $("#ping-dst").val();
  if(pingDst){
	showSpinner();
	ping(pingDst);
  }else{
	showMsg("<strong>FAILED</strong><br/> To check connectivity you must enter a valid IP or URL.");
  }
}