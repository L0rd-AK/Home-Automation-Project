// Firebase config
    const firebaseConfig = {
    apiKey: "AIzaSyCRq3UTh6v_KWMepAT3kIkOEYMUf3j6eK0",
    authDomain: "homeautomation-49080.firebaseapp.com",
    projectId: "homeautomation-49080",
    storageBucket: "homeautomation-49080.firebasestorage.app",
    messagingSenderId: "377136097626",
    appId: "1:377136097626:web:8ab36f1757bd0a40a96b63"
    };

    firebase.initializeApp(firebaseConfig);

    // Predefined login credentials (can be replaced with your logic)
    const allowedEmail = "amit@gmail.com";
    const allowedPassword = "amit1234";

    const loginForm = document.getElementById("loginForm");
    const dashboard = document.getElementById("dashboard");
    const loginBtn = document.getElementById("loginBtn");

    loginBtn.addEventListener("click", () => {
      const email = document.getElementById("email").value.trim();
      const password = document.getElementById("password").value;

      if (!email || !password) {
        alert("Please enter email and password.");
        return;
      }

      // Check hardcoded credentials first
      if(email === allowedEmail && password === allowedPassword){
        // Sign in with Firebase
        firebase.auth().signInWithEmailAndPassword(email, password)
          .then(userCredential => {
            console.log("Signed in as", userCredential.user.email);
            loginForm.style.display = "none";
            dashboard.style.display = "block";
            startRelayControl();
          })
          .catch(error => {
            alert("Firebase login failed: " + error.message);
          });
      } else {
        alert("Invalid email or password.");
      }
    });

    document.getElementById("logoutBtn").onclick = () => {
      firebase.auth().signOut()
        .then(() => {
          dashboard.style.display = "none";
          loginForm.style.display = "block";
          document.getElementById("email").value = "";
          document.getElementById("password").value = "";
        })
        .catch(error => {
          alert("Logout failed: " + error.message);
        });
    };