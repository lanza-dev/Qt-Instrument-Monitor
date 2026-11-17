using System.Globalization;
using System.IO;
using System.Net;
using System.Net.Sockets;
using System.Text;

var listener = new TcpListener(IPAddress.Loopback, 5000); // Only clients on this machine can connect
// Start listening...
listener.Start();

Console.WriteLine("Instrument simulator listening on 127.0.0.1:5000"); 

int nextClientId = 0;

// The handler starts when there is a call for it
// If it reaches an await that has not completed, it yields control to the accept loop (it can continue)

// Each client gets its own async handler. While one handler waits for a
// cmd, the server can accept another client
while (true) // It can accept another client while the first session is still open
{
    // Accepts a client
    TcpClient client = await listener.AcceptTcpClientAsync();  
    // Let this client run its session while the loop accepts other clients
    _ = HandleClientAsync(client, ++nextClientId); // // Discard the returned Task
}  

static async Task HandleClientAsync(TcpClient client, int clientId)
{
    using (client) 
    {
        Console.WriteLine($"Client {clientId} connected.");

        try
        {
            using NetworkStream stream = client.GetStream();
            using var reader = new StreamReader(stream, Encoding.ASCII, leaveOpen: true);
            using var writer = new StreamWriter(stream, Encoding.ASCII, leaveOpen: true)
            {
                // Flush each reply to the network stream 
                AutoFlush = true,
                // The protocol uses a newline to mark the end of each reply
                NewLine = "\n"
            };

            while (await reader.ReadLineAsync() is { } command) // waits for a complete line; it gives me cmd when the result is not null
            {
                if (command == "READ")
                {
                    double concentration = 120.0 + Random.Shared.NextDouble() * 10.0;
                    // Use a decimal point regardless of the machine's locale
                    string reading = concentration.ToString("F1", CultureInfo.InvariantCulture);

                    await writer.WriteLineAsync($"MEASUREMENT,{reading}");
                }
                else // Unrecognizable cmds
                {
                    await writer.WriteLineAsync("ERR,UNKNOWN_COMMAND");
                }
            }
        }
        catch (IOException ex)
        {
            Console.WriteLine($"Client {clientId} communication ended: {ex.Message}");
        }
        finally // logs/displays the disconnect; and "Using" (above) dispose of the connection and its resources
        {
            Console.WriteLine($"Client {clientId} disconnected.");
        }
    }
}