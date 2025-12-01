package academy;

import academy.cli.cmd.GenerateCommand;
import academy.cli.cmd.SolveCommand;
import academy.cli.exception.ExceptionHandler;
import java.io.File;
import picocli.CommandLine;
import picocli.CommandLine.Command;
import picocli.CommandLine.Option;

@Command(
        name = "maze-app",
        version = "maze-app 1.0",
        description = "A console utility for maze generation (DFS, Prim's) and pathfinding (A*, Dijkstra). "
                + "It provides a simple CLI to create, solve, and visualize mazes in a text-based format.",
        subcommands = {GenerateCommand.class, SolveCommand.class},
        mixinStandardHelpOptions = true)
public class Application implements Runnable {

    @Option(
            names = {"-c", "--config"},
            description = "Path to JSON config file")
    private File configPath;

    public static void main(String[] args) {
        CommandLine cmd = new CommandLine(new Application());
        cmd.setExecutionExceptionHandler(new ExceptionHandler());
        int exitCode = cmd.execute(args);
        System.exit(exitCode);
    }

    @Override
    public void run() {
        CommandLine.usage(this, System.out);
        System.setProperty("line.separator", "\n");
    }
}
