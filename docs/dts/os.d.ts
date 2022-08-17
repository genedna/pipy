declare namespace os {

  declare class Stats {
    readonly dev: number;
    readonly ino: number;
    readonly mode: number;
    readonly nlink: number;
    readonly uid: number;
    readonly gid: number;
    readonly rdev: number;
    readonly size: number;
    readonly blksize: number;
    readonly blocks: number;
    readonly atime: number;
    readonly mtime: number;
    readonly ctime: number;

    isFile(): boolean;
    isDirectory(): boolean;
    isCharacterDevice(): boolean;
    isBlockDevice(): boolean;
    isFIFO(): boolean;
    isSymbolicLink(): boolean;
    isSocket(): boolean;
  }

  /**
   * Environment variables.
   */
  declare var env: { [name: string]: string };

  /**
   * Obtains information about a file.
   */
  declare function stat(filename: string): Stats;

  /**
   * Read the entire content of a file.
   */
  declare function readFile(filename: string): Data;

  /**
   * Write the entire content of a file.
   */
  declare function writeFile(filename: string, content: Data | string): void;

}
