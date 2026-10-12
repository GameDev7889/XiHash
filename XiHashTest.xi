// Import XiHash so this program can use the language's built-in features.
import XiHash;

// Bring the Console functions into scope so Print(...) and ReadLine() can be called
// without the Console. prefix.
using namespace Console;

// Print a title and a newline so the dialogue starts on a fresh console line.
Console.Print("=== XiHash Console Chat ===\n");

// Print a prompt without a newline, leaving the cursor ready for the user's reply.
Print("What is your name? ");

// Read one line typed by the user and store that text in the string variable name.
string name = ReadLine();

// Join fixed text with the value the user typed, then print the greeting and newline.
Console.Print("Hello, " + name + "!\n");

// Ask a second question; this time Console.Print is used with its namespace prefix.
Console.Print("What do you think of XiHash? ");

// Read the user's second line and keep it in a separate string variable.
string opinion = ReadLine();

// Echo the user's answer as part of a friendly response.
Print("Thanks, " + name + ". You said: " + opinion + "\n");

// PrintC writes exactly one character; repeat it to draw a small separator.
PrintC('-');
PrintC('-');
PrintC('-');
PrintC('\n');

// Finish the dialogue with a final message and a newline.
Console.Print("Goodbye, " + name + "!\n");
