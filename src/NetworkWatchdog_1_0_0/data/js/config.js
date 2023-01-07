$(window).on("load", function() {
	hideSpinner();
	
	$("#ssid").prop('selectedIndex', -1)
	
	const form = document.getElementById( "configuration" );
	form.addEventListener( "submit", function ( event ) {
		event.preventDefault();
		showSpinner();
		saveConfiguration();
	} );
})

function detectNetworks() {
  showSpinner();
  scan();
}

function scan(){
	let xhttp = new XMLHttpRequest();
	let req = $.ajax({
	  url: "/scan",
	  type: "get",
	  headers: {
		'Access-Control-Allow-Headers': '*',
		'Access-Control-Allow-Origin': '*',
		},
	  success: function(response) {
		if(req.responseText=="wait"){
			setTimeout(function(){location.reload()},10000);
		}else{
			showMsg(req.responseText);
			return true;
		}
	  },
	  error: function(xhr) {
		showMsg("<strong>ERROR</strong><br/>Unable to scan");
	  }
	});
}

function saveConfiguration(){
	let ssidVal = $("#ssid").val();
	let ssidPassVal = $("#ssid-pass").val();
	
	let priPingVal = $("#pri-ping").val(); 
	let secPingVal = $("#sec-ping").val(); 
			
	
	// handle no WiFi Network selection
	if (ssidVal === '') {
		showMsg("<strong>ERROR</strong><br/>Please select a wireless network");
		$("#ssid").focus();
		return false;
	}
	
	// handle empty or invalid Primary Ping Address
	if(priPingVal === '') {
		showMsg("<strong>ERROR</strong><br/>Primary Ping Address cannot be empty. Please enter a valid Address.");
		$("#pri-ping").focus();
		return false;
	}else{
		if(!(isValidIP(priPingVal) || isValidURL(priPingVal))){
			showMsg("<strong>ERROR</strong><br/>Invalid Primary Ping Address");
			return false;
		}
	}
	
	// set Secondary Ping Address to null if is not set
	if(secPingVal==='' || secPingVal==='(IP unset)'){
		secPingVal=0;
	}else{
		//check if Secondary Ping Address is valid
		if(!(isValidIP(secPingVal) || isValidURL(secPingVal))){		
			showMsg("<strong>ERROR</strong><br/>Invalid Secondary Ping Address");
			return false;
		}
	}
	let xhttp = new XMLHttpRequest();
	let req = $.ajax({
	  url: "/config",
	  type: "post",
	  headers: {
		'Access-Control-Allow-Headers': '*',
		'Access-Control-Allow-Origin': '*',
		},
      data: { 
		ssid: ssidVal,
		ssid_pass: ssidPassVal,
		pri_ping: priPingVal,
		sec_ping: secPingVal,
		},
	  success: function(response) {
		if(req.responseText=="wait"){
			setTimeout(function(){location.reload()},2000);
		}else{
			if(req.responseText==='Configuration stored'){
				showMsg("<strong>SUCCESS</strong><br/>Configuration Stored. Restart Network Watchdog to connect to your Wifi Network as a client.");
				return true;
			}else{
				showMsg(req.responseText);
				return false;
			}
		}
	  },
	  error: function(xhr) {
		showMsg("<strong>ERROR</strong><br/>Unable to save Configuration");
		return false;
	  }
	});
};
