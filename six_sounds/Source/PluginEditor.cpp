#include "PluginProcessor.h"
#include "PluginEditor.h"

// ==============================================================================
// CONSTRUCTOR
// ==============================================================================
MatrixFMSynthAudioProcessorEditor::MatrixFMSynthAudioProcessorEditor (MatrixFMSynthAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p), resizer (this, nullptr)
{
    // --------------------------------------------------------------------------
    // 1. INITIALIZE ARRAYS & COMPONENTS FIRST (Prevents Null Ptr Crashes)
    // --------------------------------------------------------------------------
    
    // Tools & Sidebar
    addAndMakeVisible(toolsGroup);
    addAndMakeVisible(saveBtn);  saveBtn.setButtonText("SAVE");
    addAndMakeVisible(loadBtn);  loadBtn.setButtonText("LOAD");
    addAndMakeVisible(initBtn);  initBtn.setButtonText("INIT");
    addAndMakeVisible(randModBtn); randModBtn.setButtonText("RND MOD");
    addAndMakeVisible(uiScaleMenu);
    
    saveBtn.onClick    = [this]{ audioProcessor.savePreset(); };
    loadBtn.onClick    = [this]{ audioProcessor.loadPreset(); };
    initBtn.onClick    = [this]{ audioProcessor.initPreset(); };
    randModBtn.onClick = [this]{ audioProcessor.randomizeModMatrix(); };
    
    uiScaleMenu.addItemList({"75%", "100%", "125%", "150%"}, 1);
    uiScaleMenu.setSelectedId(2);
    uiScaleMenu.onChange = [this]{ updateScale(); };

    // Envelopes
    addAndMakeVisible(envGroup);
    addAndMakeVisible(adsrTitle1); adsrTitle1.setText("Env 1 (Amp)", juce::dontSendNotification);
    addAndMakeVisible(adsrTitle2); adsrTitle2.setText("Env 2 (Mod)", juce::dontSendNotification);
    
    // Create Env Labels & Sliders
    juce::StringArray adsrShort = {"A", "D", "S", "R"};
    for (int i = 0; i < 8; ++i) {
        addAndMakeVisible(envLabels[i]);
        envLabels[i].setText(adsrShort[i % 4], juce::dontSendNotification);
        envLabels[i].setJustificationType(juce::Justification::centred);
        envLabels[i].setFont(juce::Font(juce::FontOptions(12.0f)));

        auto& s = (i < 4) ? adsr1Sliders[i] : adsr2Sliders[i-4];
        addAndMakeVisible(s);
        s.setSliderStyle(juce::Slider::LinearVertical);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 40, 15);
    }
    
    for (int i = 0; i < 2; ++i) {
        addAndMakeVisible(envRetrigBtn[i]);
        envRetrigBtn[i].setButtonText("Retrig");
    }

    // LFOs
    addAndMakeVisible(lfoGroup);
    juce::StringArray lfoPNames = {"Rate", "Amt", "Depth"};
    
    for (int i = 0; i < 2; ++i) {
        addAndMakeVisible(lfoLabels[i]);
        lfoLabels[i].setText("LFO " + juce::String(i + 1), juce::dontSendNotification);
        lfoLabels[i].setJustificationType(juce::Justification::centred);
        lfoLabels[i].setFont(juce::Font(juce::FontOptions(14.0f).withStyle("Bold")));

        for (int j = 0; j < 3; ++j) {
            int idx = (i * 3) + j;
            addAndMakeVisible(lfoParamLabels[idx]);
            lfoParamLabels[idx].setText(lfoPNames[j], juce::dontSendNotification);
            lfoParamLabels[idx].setJustificationType(juce::Justification::centred);
            lfoParamLabels[idx].setFont(juce::Font(juce::FontOptions(11.0f)));
        }
        
        addAndMakeVisible(lfoSyncBtn[i]); lfoSyncBtn[i].setButtonText("Sync");
        addAndMakeVisible(lfoWave[i]);    lfoWave[i].addItemList({"Sine", "Tri", "Saw", "Sqr"}, 1);
        addAndMakeVisible(lfoDirectDest[i]); lfoDirectDest[i].addItemList(audioProcessor.getModDestinations(), 1);

        addAndMakeVisible(lfoFreq[i]); 
        lfoFreq[i].setSliderStyle(juce::Slider::LinearHorizontal);
        lfoFreq[i].setTextBoxStyle(juce::Slider::TextBoxBelow, false, 45, 15);
        lfoFreq[i].setTextValueSuffix(" Hz");

        addAndMakeVisible(lfoAmt[i]);  
        lfoAmt[i].setSliderStyle(juce::Slider::LinearHorizontal);
        lfoAmt[i].setTextBoxStyle(juce::Slider::TextBoxBelow, false, 45, 15);

        addAndMakeVisible(lfoDirectDepth[i]); 
        lfoDirectDepth[i].setSliderStyle(juce::Slider::LinearHorizontal);
        lfoDirectDepth[i].setTextBoxStyle(juce::Slider::TextBoxBelow, false, 45, 15);
    }

    // Master & Visualizer
    addAndMakeVisible(masterGroup);
    addAndMakeVisible(masterSlider);
    masterSlider.setSliderStyle(juce::Slider::LinearVertical);
    masterSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
    
    addAndMakeVisible(masterLabel);
    masterLabel.setText("OUT", juce::dontSendNotification);
    masterLabel.setJustificationType(juce::Justification::centred);
    
    addAndMakeVisible(visualizerLabel);
    visualizerLabel.setText("SCOPE", juce::dontSendNotification);
    addAndMakeVisible(audioProcessor.visualizer);

    // Mod Matrix
    addAndMakeVisible(modGroup);
    // Initialize ModRows ARRAY FIRST
    for (int i = 0; i < 8; ++i) {
        modRows.add(std::make_unique<ModRow>());
        auto* r = modRows[i]; // Safe access now
        addAndMakeVisible(r->source); r->source.addItemList({"LFO 1", "LFO 2", "ModW", "Vel", "Key"}, 1);
        addAndMakeVisible(r->dest);   r->dest.addItemList(audioProcessor.getModDestinations(), 1);
        addAndMakeVisible(r->amount); r->amount.setSliderStyle(juce::Slider::LinearHorizontal);
        r->amount.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0); // Cleaner look
    }

    // Operator Matrix
    addAndMakeVisible(matrixGroup);
    juce::StringArray opHeaders = {"VOL", "ENV", "RATIO", "MODE", "C1", "C2", "TRK", "MATRIX", "OUT"};
    for (int i = 0; i < 9; ++i) {
        addAndMakeVisible(opHeaderLabels[i]);
        opHeaderLabels[i].setText(opHeaders[i], juce::dontSendNotification);
        opHeaderLabels[i].setJustificationType(juce::Justification::centred);
        opHeaderLabels[i].setFont(juce::Font(juce::FontOptions(12.0f).withStyle("Bold")));
    }

    // --- INITIALIZE OP CONTROLS LOOP ---
    for (int i = 0; i < 6; ++i) {
        juce::String opId = "op" + juce::String(i);

        // Row Label & Basic Controls (Gain, Env, Ratio, Mode)
        auto* rl = rowLabels.add(std::make_unique<juce::Label>());
        addAndMakeVisible(rl);
        rl->setText("OP " + juce::String(i + 1), juce::dontSendNotification);

        auto* g = opGains.add(std::make_unique<juce::Slider>()); addAndMakeVisible(g);
        g->setSliderStyle(juce::Slider::LinearVertical);
        sAtts.add(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, opId + "gain", *g));

        auto* e = opEnvSelect.add(std::make_unique<juce::ComboBox>()); addAndMakeVisible(e);
        e->addItemList({ "Env 1", "Env 2" }, 1);
        cAtts.add(std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.apvts, opId + "envSrc", *e));

        auto* r = opRatios.add(std::make_unique<juce::Slider>()); addAndMakeVisible(r);
        r->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        r->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 45, 15);
        sAtts.add(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, opId + "ratio", *r));

        auto* m = opModes.add(std::make_unique<juce::ComboBox>()); addAndMakeVisible(m);
        m->addItemList({ "Sine", "LP", "Comb" }, 1);
        cAtts.add(std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.apvts, opId + "mode", *m));

        // --- Dynamic Knobs (C1, C2, TRK) ---
        auto* c1 = opContext1.add(std::make_unique<juce::Slider>()); addAndMakeVisible(c1);
        auto* c2 = opContext2.add(std::make_unique<juce::Slider>()); addAndMakeVisible(c2);
        auto* trk = opTrack.add(std::make_unique<juce::Slider>());   addAndMakeVisible(trk);

        // Setup styles
        for (auto* s : {c1, c2, trk}) {
            s->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            s->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        }

        // Dynamic Labels (Add these to the UI)
        auto* L1 = opContextLabels1.add(std::make_unique<juce::Label>()); addAndMakeVisible(L1);
        auto* L2 = opContextLabels2.add(std::make_unique<juce::Label>()); addAndMakeVisible(L2);
        auto* LT = opTrackLabels.add(std::make_unique<juce::Label>());   addAndMakeVisible(LT);
        for (auto* l : {L1, L2, LT}) {
            l->setJustificationType(juce::Justification::centred);
            l->setFont(juce::Font(juce::FontOptions(10.0f)));
        }

        // Attachments
        sAtts.add(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, opId + "res", *c1));
        sAtts.add(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, opId + "res2", *c2));
        sAtts.add(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, opId + "kbtrack", *trk));

        // Matrix & Output
        for (int j = 0; j < 6; ++j) {
            auto* k = matrixKnobs.add(std::make_unique<juce::Slider>()); addAndMakeVisible(k);
            k->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            sAtts.add(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "m" + juce::String(i) + juce::String(j), *k));
        }
        auto* go = opGainsOut.add(std::make_unique<juce::Slider>()); addAndMakeVisible(go);
        go->setSliderStyle(juce::Slider::LinearVertical);
        sAtts.add(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, opId + "gainOut", *go));

        // --- Dynamic Logic Callback ---
        m->onChange = [this, i, L1, L2, LT, c1, c2, trk] {
            auto mode = opModes[i]->getText();
            
            if (mode == "Sine") {
                L1->setText("PHASE", juce::dontSendNotification);
                L2->setText("", juce::dontSendNotification);
                LT->setText("", juce::dontSendNotification);
                c2->setVisible(false);
                trk->setVisible(false);
            } 
            else if (mode == "LP") {
                L1->setText("CUTOFF", juce::dontSendNotification);
                L2->setText("RESO", juce::dontSendNotification);
                LT->setText("TRACK", juce::dontSendNotification);
                c2->setVisible(true);
                trk->setVisible(true);
            } 
            else if (mode == "Comb") {
                L1->setText("DELAY", juce::dontSendNotification);
                L2->setText("FEEDBK", juce::dontSendNotification);
                LT->setText("TRACK", juce::dontSendNotification);
                c2->setVisible(true);
                trk->setVisible(true);
            }
        };
        m->onChange(); 
    }

    addAndMakeVisible(resizer);

    // --------------------------------------------------------------------------
    // 2. CREATE ATTACHMENTS (Connect Audio Last)
    // --------------------------------------------------------------------------
    
    // Master
    masterAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "master_out", masterSlider);

    // Envelopes
    juce::StringArray envParams = {"attack", "decay", "sustain", "release"};
    for (int i = 0; i < 4; ++i) {
        sAtts.add(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, envParams[i], adsr1Sliders[i]));
        sAtts.add(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "a2_" + envParams[i], adsr2Sliders[i]));
    }
    bAtts.add(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.apvts, "env1ret", envRetrigBtn[0]));
    bAtts.add(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.apvts, "env2ret", envRetrigBtn[1]));

    // LFOs
    for (int i = 0; i < 2; ++i) {
        juce::String id = "lfo" + juce::String(i);
        bAtts.add(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.apvts, id + "sync", lfoSyncBtn[i]));
        cAtts.add(std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.apvts, id + "wave", lfoWave[i]));
        sAtts.add(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, id + "freq", lfoFreq[i]));
        sAtts.add(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, id + "amt", lfoAmt[i]));
        cAtts.add(std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.apvts, id + "directDest", lfoDirectDest[i]));
        sAtts.add(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, id + "directDepth", lfoDirectDepth[i]));
    }

    // Mod Matrix
    for (int i = 0; i < 8; ++i) {
        juce::String pId = "slot" + juce::String(i);
        auto* r = modRows[i];
        r->srcAtt  = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.apvts, pId + "src",  r->source);
        r->destAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.apvts, pId + "dest", r->dest);
        r->amtAtt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>  (p.apvts, pId + "amt",  r->amount);
    }

    // Operator Matrix
    for (int i = 0; i < 6; ++i) {
        juce::String op = "op" + juce::String(i);
        cAtts.add(std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.apvts, op + "mode",    *opModes[i]));
        cAtts.add(std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.apvts, op + "envSrc",  *opEnvSelect[i]));
        sAtts.add(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>  (p.apvts, op + "ratio",   *opRatios[i]));
        sAtts.add(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>  (p.apvts, op + "gain",    *opGains[i]));
        sAtts.add(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>  (p.apvts, op + "gainOut", *opGainsOut[i]));
        sAtts.add(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>  (p.apvts, op + "res",     *opContext1[i]));
        sAtts.add(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>  (p.apvts, op + "res2",     *opContext2[i]));
        sAtts.add(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>  (p.apvts, op + "kbtrack", *opTrack[i]));
        
        for (int j = 0; j < 6; ++j) {
            // Note: m + row + col (e.g., m01)
            sAtts.add(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "m" + juce::String(i) + juce::String(j), *matrixKnobs[i * 6 + j]));
        }
    }

    // --------------------------------------------------------------------------
    // 3. TRIGGER LAYOUT (Final Step)
    // --------------------------------------------------------------------------
    setSize(1000, 850); 
}

// ==============================================================================
// DESTRUCTOR & HELPERS
// ==============================================================================
MatrixFMSynthAudioProcessorEditor::~MatrixFMSynthAudioProcessorEditor() {
    // Attachments are unique_ptrs, so they clean themselves up automatically.
}

void MatrixFMSynthAudioProcessorEditor::paint (juce::Graphics& g) {
    g.fillAll(juce::Colour(30, 30, 35)); // Dark Grey Background
}

void MatrixFMSynthAudioProcessorEditor::updateScale() {
    float s = 1.0f;
    switch(uiScaleMenu.getSelectedId()) {
        case 1: s = 0.75f; break;
        case 2: s = 1.0f;  break;
        case 3: s = 1.25f; break;
        case 4: s = 1.5f;  break;
    }
    setTransform(juce::AffineTransform::scale(s));
    setSize(1000 * s, 850 * s);
}

// ==============================================================================
// RESIZED (Layout Logic)
// ==============================================================================
void MatrixFMSynthAudioProcessorEditor::resized() {
    auto area = getLocalBounds().withSize(1000, 850).reduced(10);
    auto header = area.removeFromTop(250);

    // Sidebar
    auto sidebar = header.removeFromLeft(100);
    toolsGroup.setBounds(sidebar.reduced(2));
    auto sb = toolsGroup.getBounds().reduced(10, 20);
    saveBtn.setBounds(sb.removeFromTop(30)); sb.removeFromTop(5);
    loadBtn.setBounds(sb.removeFromTop(30)); sb.removeFromTop(5);
    initBtn.setBounds(sb.removeFromTop(30)); sb.removeFromTop(5);
    randModBtn.setBounds(sb.removeFromTop(30)); sb.removeFromTop(10);
    uiScaleMenu.setBounds(sb.removeFromTop(25));

    // Center
    auto center = header.removeFromLeft(header.getWidth() * 0.75f);
    
    // Env Layout
    envGroup.setBounds(center.removeFromTop(120).reduced(2));
    auto eA = envGroup.getBounds().reduced(10, 5);
    auto e1 = eA.removeFromLeft(eA.getWidth()/2);
    adsrTitle1.setBounds(e1.removeFromTop(18));
    envRetrigBtn[0].setBounds(e1.removeFromTop(15));
    for (int i=0; i<4; ++i) {
        auto col = e1.removeFromLeft(e1.getWidth()/(4-i));
        envLabels[i].setBounds(col.removeFromTop(15));
        adsr1Sliders[i].setBounds(col);
    }
    adsrTitle2.setBounds(eA.removeFromTop(18));
    envRetrigBtn[1].setBounds(eA.removeFromTop(15));
    for (int i=0; i<4; ++i) {
        auto col = eA.removeFromLeft(eA.getWidth()/(4-i));
        envLabels[i+4].setBounds(col.removeFromTop(15));
        adsr2Sliders[i].setBounds(col);
    }

    // LFO Layout
    lfoGroup.setBounds(center.reduced(2));
    auto lA = lfoGroup.getBounds().reduced(10, 5);
    for (int i=0; i<2; ++i) {
        auto r = lA.removeFromLeft(lA.getWidth()/(2-i)).reduced(5,0);
        lfoLabels[i].setBounds(r.removeFromTop(18));
        auto row1 = r.removeFromTop(22);
        lfoSyncBtn[i].setBounds(row1.removeFromLeft(row1.getWidth()/2));
        lfoWave[i].setBounds(row1);
        lfoDirectDest[i].setBounds(r.removeFromTop(24).reduced(0,2));
        
        auto kLabels = r.removeFromTop(15);
        auto kRow = r; 
        int kw = kRow.getWidth()/3;
        for (int j=0; j<3; ++j) {
            lfoParamLabels[(i*3)+j].setBounds(kLabels.removeFromLeft(kw));
            auto target = (j==0) ? &lfoFreq[i] : (j==1 ? &lfoAmt[i] : &lfoDirectDepth[i]);
            target->setBounds(kRow.removeFromLeft(kw));
        }
    }

    // Right Column
    masterGroup.setBounds(header.removeFromRight(75).reduced(2));
    auto mArea = masterGroup.getBounds().reduced(5);
    masterLabel.setBounds(mArea.removeFromTop(20));
    masterSlider.setBounds(mArea);
    visualizerLabel.setBounds(header.removeFromTop(20));
    audioProcessor.visualizer.setBounds(header.reduced(5));

    // Mod Matrix
    auto modArea = area.removeFromTop(180);
    modGroup.setBounds(modArea);
    auto mGrid = modArea.reduced(10, 20);
    auto mLeft = mGrid.removeFromLeft(mGrid.getWidth()/2);
    for (int i=0; i<8; ++i) {
        auto& col = (i<4) ? mLeft : mGrid;
        auto row = col.removeFromTop(col.getHeight()/(4-(i%4))).reduced(2);
        modRows[i]->source.setBounds(row.removeFromLeft(70));
        modRows[i]->dest.setBounds(row.removeFromLeft(130));
        modRows[i]->amount.setBounds(row);
    }

    // Operator Matrix
    matrixGroup.setBounds(area.reduced(0, 5));
    auto g = area.reduced(10, 10);
    
    // Header Row
    auto hRow = g.removeFromTop(25);
    hRow.removeFromLeft(40);
    opHeaderLabels[0].setBounds(hRow.removeFromLeft(30)); // Vol
    opHeaderLabels[1].setBounds(hRow.removeFromLeft(60)); // Env
    opHeaderLabels[2].setBounds(hRow.removeFromLeft(70)); // RATIO
    opHeaderLabels[3].setBounds(hRow.removeFromLeft(80)); // Mode
    opHeaderLabels[4].setBounds(hRow.removeFromLeft(40)); // C1
    opHeaderLabels[5].setBounds(hRow.removeFromLeft(40)); // C2
    opHeaderLabels[6].setBounds(hRow.removeFromLeft(40)); // Trk
    opHeaderLabels[7].setBounds(hRow.removeFromLeft(hRow.getWidth()-40)); // Matrix
    opHeaderLabels[8].setBounds(hRow); // Out

    int rH = g.getHeight() / 6;
    for (int i = 0; i < 6; ++i) {
        auto row = g.removeFromTop(rH);
        rowLabels[i]->setBounds(row.removeFromLeft(40));
        opGains[i]->setBounds(row.removeFromLeft(30));
        opEnvSelect[i]->setBounds(row.removeFromLeft(60).reduced(2, 8));
        opRatios[i]->setBounds(row.removeFromLeft(70)); 
        opModes[i]->setBounds(row.removeFromLeft(80).reduced(2, 8));
        opContext1[i]->setBounds(row.removeFromLeft(40));
        opContext2[i]->setBounds(row.removeFromLeft(40));
        opTrack[i]->setBounds(row.removeFromLeft(40));
        auto mZone = row.removeFromLeft(row.getWidth() - 40);
        for (int j = 0; j < 6; ++j) matrixKnobs[i * 6 + j]->setBounds(mZone.removeFromLeft(mZone.getWidth() / (6 - j)).reduced(1));
        opGainsOut[i]->setBounds(row);
    }
}
