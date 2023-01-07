$(window).on("load", function() {
	const bootTimeElement = $( "#boot-time" );
	if(bootTimeElement.text()=="0"){
		bootTimeElement.text('Not available');
		bootTimeElement.text('Not available');
	}else{
		let epochTime = new Date(0); // set the date to the epoch
		let curTime = new Date();
		epochTime.setUTCSeconds(bootTimeElement.text());
		bootTimeElement.text(epochTime);
		let bootTime = Date.parse(epochTime);
		
		let seconds = Math.floor((curTime - (bootTime))/1000);
		let minutes = Math.floor(seconds/60);
		let hours = Math.floor(minutes/60);
		let days = Math.floor(hours/24);

		hours = hours-(days*24);
		minutes = minutes-(days*24*60)-(hours*60);
		seconds = seconds-(days*24*60*60)-(hours*60*60)-(minutes*60);
		
		//pad zeros accordingly
		$("#uptime").text(('0' + days).slice(-2)+':'+ ('0' + hours).slice(-2)+':'+('0' + minutes).slice(-2)+':'+('0' + seconds).slice(-2));
	}
	$(".spinner").fadeOut();
})