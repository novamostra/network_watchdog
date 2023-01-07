function showSpinner() {
  $(".spinner").fadeIn()
}

function hideSpinner() {
  $(".spinner").fadeOut()
}	

function showMsg(err) {
  $(".spinner").hide();
  modalMessage.innerHTML=err
  modal.style.display = "block";  
}

const modal = document.getElementById("modal-window");

let modalMessage = document.getElementById("modal-message");

// When the user clicks anywhere outside of the modal, close it
window.onclick = function(event) {
  if (event.target == modal) {
	modal.style.display = "none";
  }
} 