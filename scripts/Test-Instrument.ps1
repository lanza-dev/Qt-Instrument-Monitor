# Connect to the simulator running on this computer.
$client = [System.Net.Sockets.TcpClient]::new('127.0.0.1', 5000)

try {
    $stream = $client.GetStream()
    $stream.ReadTimeout = 3000

    # Send one command. The newline marks the end of the command.
    $command = [System.Text.Encoding]::ASCII.GetBytes("READ`n")
    $stream.Write($command, 0, $command.Length)

    # Read and display one complete reply.
    $reader = [System.IO.StreamReader]::new($stream)
    $reader.ReadLine()
    }
finally 
{
    # Close this client's connection when the test finishes.
    $client.Dispose()
}
