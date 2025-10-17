
function connect() {
    // Create WebSocket connection.
    let socket;
    
    try {
        socket = new WebSocket("ws://127.0.0.1:80");
        console.log("Initialized socket")
    } catch (e) {
        console.log("Failed to initialize socket");
        console.error(e);
        return;
    }

    // Connection opened
    socket.addEventListener("open", (event) => {
        console.log("Opened connection");
        socket.send("Hello Server!");
    });

    // Listen for messages
    socket.addEventListener("message", (event) => {
        console.log("Message from server ", event.data);
    });
}
