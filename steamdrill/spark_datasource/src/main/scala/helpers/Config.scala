/*
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package helpers

object Config {

  var hosts : Seq[String] = Nil
  var timerFile : String = ""

  val numCores : Int = sys.env("STEAMDRILL_NUM_CORES").toInt

  private var shidx : Int = 0
  def getGenTag : Int = synchronized {
    shidx += 1
    shidx
  }

	def get_tag : String = sys.env("STEAMDRILL_TAG")


  ////
  // All of these are just for the formatting
  def getEpochBase(replay: String, round: Int, numEpochs: Int, epochIdx: Int) : String = {
    s"/dev/shm/$get_tag.${replay.replaceAll("/", ".").substring(1)}-$round-$numEpochs-$epochIdx"
  }
  def getRoundMetadata(replay : String, round : Int, epochs : Int, idx : Int): String = {
    s"${getEpochBase(replay, round, epochs, idx)}-meta"
  }
  def getRoundStats(replay : String, round : Int, epochs : Int, idx : Int): String = {
    s"${getEpochBase(replay, round, epochs, idx)}-stats"
  }
  def getRoundStdout(replay : String, round : Int, epochs : Int, idx : Int): String = {
    s"${getEpochBase(replay, round, epochs, idx)}-out"
  }
  def getRoundStderr(replay : String, round : Int, epochs : Int, idx : Int): String = {
    s"${getEpochBase(replay, round, epochs, idx)}-err"
  }

  def getCommands(replay : String, round : Int, epochs : Int, idx : Int, otNum : Int): String = {
    s"/dev/shm/${getEpochBase(replay, round, epochs, idx).replaceAll("/", ".").substring(1)}-$otNum-commands"
  }

  def getShmName(replay : String, round : Int, epochs : Int, idx : Int, otNum : Int): String = {
    s"${getCommands(replay, round, epochs, idx, otNum)}-output"
  }

  def getJumpCounterType: String = {
    sys.env("STEAMDRILL_JUMP_COUNTER")
  }

  def getLogLevel: String = {
    sys.env("STEAMDRILL_LOG_LEVEL")
  }

  def getAssemblyReorder: Boolean = {
			 sys.env("STEAMDRILL_ASSEMBLY_REORDER").startsWith("T")
  }

  def genStats(name : String): String = {
    s"/dev/shm/$get_tag-$name-$getGenTag-genstats"
  }

  def staticFile(file: String) : String = {
    s"${sys.env("STEAMDRILL_INSTALL_DIR")}/../static_tables/$file"
  }

  def SteamdrillProcess() : String = {
    s"${sys.env("STEAMDRILL_INSTALL_DIR")}/../server/steamdrill_process"
  }

  def LdLibraryPath() : String = {
    val install = sys.env("STEAMDRILL_INSTALL_DIR")
    s"$install/../lib/elfutils/install/lib;$install/../lib/capstone"
  }

}
