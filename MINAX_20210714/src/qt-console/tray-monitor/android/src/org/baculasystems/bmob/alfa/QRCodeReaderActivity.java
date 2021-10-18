/*
   Bacula(R) - The Network Backup Solution

   Copyright (C) 2000-2020 Kern Sibbald

   The original author of Bacula is Kern Sibbald, with contributions
   from many others, a complete list can be found in the file AUTHORS.

   You may use this file and others of this release according to the
   license defined in the LICENSE file, which includes the Affero General
   Public License, v3.0 ("AGPLv3") and some additional permissions and
   terms pursuant to its AGPLv3 Section 7.

   This notice must be preserved when any source code is
   conveyed and/or propagated.

   Bacula(R) is a registered trademark of Kern Sibbald.
*/

package org.baculasystems.bmob.alfa;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.util.SparseArray;
import androidx.appcompat.app.AppCompatActivity;
import com.google.android.gms.vision.barcode.Barcode;
import java.util.List;
import info.androidhive.barcode.BarcodeReader;

/*
    QRCodeReaderActivity - Screen created to scan QR Codes.
    (Requires camera permission to work)
*/
public class QRCodeReaderActivity extends AppCompatActivity implements BarcodeReader.BarcodeReaderListener {

    // Called by our c++ code.
    static void start(Context ctx) {
        Intent it = new Intent(ctx, QRCodeReaderActivity.class);
        ctx.startActivity(it);
    }

    private BarcodeReader barcodeReader;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.layout_barcode);
        barcodeReader = (BarcodeReader) getSupportFragmentManager().findFragmentById(R.id.barcode_fragment);
    }


    @Override
    public void onScanned(final Barcode barcode) {
        barcodeReader.playBeep();
        Intent returnIntent = new Intent();
        // Returns the scanned data to our c++ code.
        returnIntent.putExtra("barcode.data", barcode.displayValue);
        setResult(Activity.RESULT_OK, returnIntent);
        finish();
    }

    @Override
    public void onScannedMultiple(List<Barcode> list) {}

    @Override
    public void onBitmapScanned(SparseArray<Barcode> sparseArray) {}

    @Override
    public void onScanError(String s) {}

    @Override
    public void onCameraPermissionDenied() {
        Intent returnIntent = new Intent();
        setResult(Activity.RESULT_CANCELED, returnIntent);
        finish();
    }
}