        private void eraseToolStripMenuItem_Click(object sender, EventArgs e)
        {
            if (true != usb.checkCommunication())
            {
                richTextBox1.AppendText("USB is not connected!\r\n");
                return;
            }

            byte[] dataArray = new byte[64];
            dataArray[0] = KONST.ERASE_FLASH;
            if (usb.writeUSB(dataArray))
            {
            }
        }

        private void readToolStripMenuItem_Click(object sender, EventArgs e)
        {
            if (true != usb.checkCommunication())
            {
                richTextBox1.AppendText("USB is not connected!\r\n");
                return;
            }

            byte[] dataArray = new byte[64];
            dataArray[0] = KONST.READ_BL_VERSION;
            if (usb.writeUSB(dataArray))
            {
            }
        }

        private bool downloadFirmware(string fileName)
        {
            UInt16 i = 0;
            String tempStr = "";
            UInt16 HiFlashAddress = 0;
            try
            {
                FileInfo hexFile = new FileInfo(fileName);
                TextReader hexRead = hexFile.OpenText();
                byte[] flashWriteData = new byte[1 + 4 + 32];

                string fileLine = hexRead.ReadLine();
                while (fileLine != null)
                {
                    if ((fileLine[0] == ':') && (fileLine.Length >= 11))
                    {
                        int byteCount = Int32.Parse(fileLine.Substring(1, 2), System.Globalization.NumberStyles.HexNumber);
                        int fileAddress = Int32.Parse(fileLine.Substring(3, 4), System.Globalization.NumberStyles.HexNumber);
                        int recordType = Int32.Parse(fileLine.Substring(7, 2), System.Globalization.NumberStyles.HexNumber);

                        if(recordType == 0x04)
                        {
                            HiFlashAddress = UInt16.Parse(fileLine.Substring(9, 4), System.Globalization.NumberStyles.HexNumber);
                        }

                        if (recordType == 0)
                        {
                            flashWriteData[0] = KONST.PROGRAM_FLASH;
                            UInt32 flashaddress = (UInt32)((HiFlashAddress << 16) | fileAddress);
                            if (flashaddress >= 0x4800 && flashaddress < 0x28000)
                            {
                                flashWriteData[1] = (byte)(byteCount + 4);
                                flashWriteData[2] = (byte)(flashaddress & 0xFF);
                                flashWriteData[3] = (byte)((flashaddress >> 8) & 0xFF);
                                flashWriteData[4] = (byte)((flashaddress >> 16) & 0xFF);
                                flashWriteData[5] = (byte)((flashaddress >> 24) & 0xFF);

                                for (i = 0; i < byteCount; i++)
                                {
                                    flashWriteData[6 + i] = (byte)(UInt16.Parse(fileLine.Substring((9 + (2 * i)), 2),
                                        System.Globalization.NumberStyles.HexNumber));
                                }
                                for (i = 0; i < byteCount + 4; i++)
                                {
                                    tempStr += flashWriteData[2 + i].ToString("X2");
                                    tempStr += " ";
                                }
                                tempStr += "\n";
                                //richTextBox1.AppendText(tempStr);

                                if (usb.writeUSB(flashWriteData))
                                {

                                }
                                tempStr = "";
                            }
                        }

                        if (recordType == 1)
                        {
                            break;
                        }

                    }
                    fileLine = hexRead.ReadLine();
                }
                MessageBox.Show("Download data OK!", "OK", MessageBoxButtons.OK, MessageBoxIcon.Information);
                hexRead.Close();
                return true;
            }
            catch
            {
                return false;
            }
        }
        private void programToolStripMenuItem1_Click(object sender, EventArgs e)
        {
            OpenFileDialog dialog = new OpenFileDialog();
            dialog.Filter = "|*.hex";
            if (true != usb.checkCommunication())
            {
                richTextBox1.AppendText("No device detected!");
                return;
            }

            if (dialog.ShowDialog() == DialogResult.OK)
            {
                string fileName = dialog.FileName;
                backgroundWorkerOTA.RunWorkerAsync(fileName);
                //downloadFirmware(dialog.FileName);
            }
        }


        private void backgroundWorkerOTA_DoWork(object sender, DoWorkEventArgs e)
        {
            string fileName = (string)e.Argument;
            UInt16 i = 0;
            UInt16 HiFlashAddress = 0;
            try
            {
                FileInfo hexFile = new FileInfo(fileName);
                TextReader hexlineRead = hexFile.OpenText();
                int lines = 0;
                while(hexlineRead.ReadLine() != null)
                {
                    lines++;
                }
                hexlineRead.Close();
                TextReader hexRead = hexFile.OpenText();
                byte[] flashWriteData = new byte[1 + 4 + 32];
                string fileLine = hexRead.ReadLine();
                int k = 0;
                while (fileLine != null)
                {
                    k++;

                    if ((fileLine[0] == ':') && (fileLine.Length >= 11))
                    {
                        int byteCount = Int32.Parse(fileLine.Substring(1, 2), System.Globalization.NumberStyles.HexNumber);
                        int fileAddress = Int32.Parse(fileLine.Substring(3, 4), System.Globalization.NumberStyles.HexNumber);
                        int recordType = Int32.Parse(fileLine.Substring(7, 2), System.Globalization.NumberStyles.HexNumber);

                        if (recordType == 0x04)
                        {
                            HiFlashAddress = UInt16.Parse(fileLine.Substring(9, 4), System.Globalization.NumberStyles.HexNumber);
                        }

                        if (recordType == 0)
                        {
                            flashWriteData[0] = KONST.PROGRAM_FLASH;
                            UInt32 flashaddress = (UInt32)((HiFlashAddress << 16) | fileAddress);
                            if (flashaddress >= 0x4800 && flashaddress < 0x28000)
                            {
                                flashWriteData[1] = (byte)(byteCount + 4);
                                flashWriteData[2] = (byte)(flashaddress & 0xFF);
                                flashWriteData[3] = (byte)((flashaddress >> 8) & 0xFF);
                                flashWriteData[4] = (byte)((flashaddress >> 16) & 0xFF);
                                flashWriteData[5] = (byte)((flashaddress >> 24) & 0xFF);

                                for (i = 0; i < byteCount; i++)
                                {
                                    flashWriteData[6 + i] = (byte)(UInt16.Parse(fileLine.Substring((9 + (2 * i)), 2),
                                        System.Globalization.NumberStyles.HexNumber));
                                }

                                usb.writeUSB(flashWriteData);
                            }
                        }

                        backgroundWorkerOTA.ReportProgress((100 * k) / lines);

                        if (recordType == 1)
                        {
                            break;
                        }

                    }
                    fileLine = hexRead.ReadLine();
                }
                //MessageBox.Show("Download data OK!", "OK", MessageBoxButtons.OK, MessageBoxIcon.Information);
                hexRead.Close();
            }
            catch
            {

            }
        }

        private void backgroundWorkerOTA_ProgressChanged(object sender, ProgressChangedEventArgs e)
        {
            progressBarStatus.Value = e.ProgressPercentage;
        }

        private void backgroundWorkerOTA_RunWorkerCompleted(object sender, RunWorkerCompletedEventArgs e)
        {
            richTextBox1.AppendText("Firmware update finish\r\n");
        }