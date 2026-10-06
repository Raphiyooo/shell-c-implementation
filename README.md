# Shell Implementation in C

This project is about recreating functionalities of a shell, building the terminal around it and putting it on a website.

Since AI can enhance the speed and quality, my workflow is as follows: First, I think about how I could implement the new feature: How could I design it such that it works well with the current implementation? What concepts do I need to build this feature? Can I reuse other functions? After I got a few ideas on how I could implement it, I will write it out, with this I get a clear mind and, moreover, find possible difficulties or things that would work well. I will give that text to an AI (usually GPT 6 Sol or similar) and ask it to give its opinion on what would make the most sense and which will give the best time/space complexity. Once I get the perspective of the AI, I will once again write it out so I can think about it. After all, I will implement it by myself. For these type of projects I dont use AI to write code, since I really want to learn and think about the implementation.

## Development steps

I began with building the basic shell commands such as echo, cd, cat, type and pwd. I started with the question on how to read user input, first I always looked just at the first word,
the rest of the input I put into another char pointer.
However, I needed a way to analyse the quotes, double quotes, backslash within quotes and so on. So I rebuilt the input reading mechanism to
save the input in a 2D char array, this way I can loop over the tokens in the array and can process each word one by another.

## What I learned
- Making a program working for every operating system
- Using file descriptors
- Process creation/execution
- Environment variables/PATH lookup
- Complex parsing/tokenization
- Treminal control with termios

## Problems I encountered
1. Handling difficult inputs including any form of quotes. 
How did I decide to do this?
I rethought my initial idea of reading user input by drawing my current system out on a blank sheet of paper. With this, I got a good overview of how I am currently reading in input and
how I should do it. I implemented two versions before, however I saw that reading user input key by key is optimal for a shell. For that I had to change the terminal mode to *non canonical mode* and turn off the "ECHO" flag and write every keystroke to the terminal with the write() function.

2. Managing ownership of dynamic