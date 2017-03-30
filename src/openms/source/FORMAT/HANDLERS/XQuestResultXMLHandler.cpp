// --------------------------------------------------------------------------
//                   OpenMS -- Open-Source Mass Spectrometry
// --------------------------------------------------------------------------
// Copyright The OpenMS Team -- Eberhard Karls University Tuebingen,
// ETH Zurich, and Freie Universitaet Berlin 2002-2016.
//
// This software is released under a three-clause BSD license:
//  * Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//  * Neither the name of any author or any participating institution
//    may be used to endorse or promote products derived from this software
//    without specific prior written permission.
// For a full list of authors, refer to the file AUTHORS.
// --------------------------------------------------------------------------
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL ANY OF THE AUTHORS OR THE CONTRIBUTING
// INSTITUTIONS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
// OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
// ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// --------------------------------------------------------------------------
// $Maintainer: Lukas Zimmermann $
// $Authors: Lukas Zimmermann $
// --------------------------------------------------------------------------

#include <OpenMS/FORMAT/HANDLERS/XQuestResultXMLHandler.h>
#include <xercesc/sax2/Attributes.hpp>
#include <OpenMS/METADATA/XQuestResultMeta.h>
#include <OpenMS/METADATA/PeptideIdentification.h>
#include <OpenMS/METADATA/ProteinIdentification.h>
#include <OpenMS/METADATA/PeptideHit.h>
#include <OpenMS/CONCEPT/LogStream.h>
#include <boost/assign/list_of.hpp>

#include <iostream>
#include <utility>

using namespace std;
using namespace xercesc;

// TODO Switch to Debug mode in CMake and remove this undef
#undef NDEBUG
#include <assert.h>

namespace OpenMS
{
  namespace Internal
  {

    /*
     *  Helper functions
     */
    void removeSubstring(String &  large, const String & small)
    {
      std::string::size_type i = large.find(small);
      if (i != std::string::npos)
      {
        large.erase(i, small.length());
      }
    }
   
    
   // Initialize static const members
  
   const std::map< Size, String > XQuestResultXMLHandler::enzymes = boost::assign::map_list_of(0, "no_enzyme") 
   (1, "trypsin") (2, "chymotrypsin") (3, "unknown_enzyme") (9, "unknown_enzyme")
   (10, "unknown_enzyme") (14, "unknown_enzyme") (15, "unknown_enzyme") (16, "unknown_enzyme") (17, "unknown_enzyme")
   (18, "unknown_enzyme") (20, "unknown_enzyme");
  
  
    XQuestResultXMLHandler::XQuestResultXMLHandler(const String &filename,
                                                   vector< XQuestResultMeta> & metas,
                                                   std::vector< std::vector< PeptideIdentification > > & csms,
                                                   std::vector< ProteinIdentification > & prot_ids,
                                                   int & n_hits_,
                                                   std::vector< int > * cum_hits,
                                                   size_t min_n_ions_per_spectrum,
                                                   bool load_to_peptideHit) :
      XMLHandler(filename, "1.0"),
      metas_(metas),
      csms_(csms),
      prot_ids_(prot_ids),
      n_hits_(n_hits_),
      cum_hits_(cum_hits),
      min_n_ions_per_spectrum_(min_n_ions_per_spectrum),
      load_to_peptideHit_(load_to_peptideHit)
    {
      // Initialize the one and only protein identification
      this->prot_ids_.clear();
      ProteinIdentification prot_id;
      this->prot_ids_.push_back(prot_id);
      
      // Fetch the enzymes database
      this->enzymes_db = EnzymesDB::getInstance();
    }


    XQuestResultXMLHandler::~XQuestResultXMLHandler()
    {

    }


    // Extracts the position of the Cross-Link for intralinks and crosslinks
    void XQuestResultXMLHandler::getLinkPosition_(const xercesc::Attributes & attributes, std::pair<SignedSize, SignedSize> & pair)
    {
      String xlink_position = this->attributeAsString_(attributes, "xlinkposition");
      StringList xlink_position_split;
      StringUtils::split(xlink_position, "," ,xlink_position_split);
      assert(xlink_position_split.size() == 2);

      pair.first = xlink_position_split[0].toInt();
      pair.second = xlink_position_split[1].toInt();
    }
    
    void XQuestResultXMLHandler::setPeptideEvidence_(const String & prot_string, PeptideHit & pep_hit)
    {
      StringList prot_list;
      StringUtils::split(prot_string, ",", prot_list);
      vector< PeptideEvidence > evidences(prot_list.size());
      
      for( StringList::const_iterator prot_list_it = prot_list.begin();
               prot_list_it != prot_list.end(); ++prot_list_it)
      {
        PeptideEvidence pep_ev;
        String accession = *prot_list_it; 
        
        if (this->accessions.find(accession) == this->accessions.end())
        {
          this->accessions.insert(accession);
          
          ProteinHit prot_hit;
          prot_hit.setAccession(accession);
          prot_hit.setMetaValue("target_decoy", accession.hasSubstring("decoy") ? "decoy" : "target");
          
          this->prot_ids_[0].getHits().push_back(prot_hit);
        }
       
        pep_ev.setProteinAccession(accession);
        pep_ev.setStart(PeptideEvidence::UNKNOWN_POSITION); // These information are not available in the xQuest result file
        pep_ev.setEnd(PeptideEvidence::UNKNOWN_POSITION);
        pep_ev.setAABefore(PeptideEvidence::UNKNOWN_AA);
        pep_ev.setAAAfter(PeptideEvidence::UNKNOWN_AA);
        
        evidences.push_back(pep_ev);
      }
      pep_hit.setPeptideEvidences(evidences);
    }
  

    // Assign all attributes in the peptide_id_attributes map to the MetaInfoInterface object
    void XQuestResultXMLHandler::add_meta_values(MetaInfoInterface & meta_info_interface)
    {
      for (std::map<String, DataValue>::const_iterator it = this->peptide_id_meta_values.begin();
           it != this->peptide_id_meta_values.end(); it++)
      {
        std::pair<String, DataValue> item = *it;
        meta_info_interface.setMetaValue(item.first, item.second);
      }
    }

    void XQuestResultXMLHandler::setMetaValue(const String &  key , const DataValue &  datavalue , PeptideIdentification & pep_id, PeptideHit & alpha)
    {
      pep_id.setMetaValue(key, datavalue);
      if (this->load_to_peptideHit_)
      {
        alpha.setMetaValue(key, datavalue);
      }
    }
    void XQuestResultXMLHandler::setMetaValue(const String &  key , const DataValue &  datavalue , PeptideIdentification & pep_id, PeptideHit & alpha, PeptideHit & beta)
    {
      this->setMetaValue(key, datavalue, pep_id, alpha);
      if (this->load_to_peptideHit_)
      {
        beta.setMetaValue(key, datavalue);
      }
    }


    void XQuestResultXMLHandler::endElement(const XMLCh * const /*uri*/, const XMLCh * const /*local_name*/, const XMLCh * const qname)
    {
      String tag = XMLString::transcode(qname);
      if (tag == "spectrum_search")
      {
          // Push back spectrum search vector
          size_t current_spectrum_size = this->current_spectrum_search.size();
          if (current_spectrum_size >= this->min_n_ions_per_spectrum_)
          {
           /* Currently does not work
            vector< PeptideIdentification > newvec(current_spectrum_size);
            for(vector< PeptideIdentification>::const_iterator it = this->current_spectrum_search.begin();
                it != this->current_spectrum_search.end(); ++it)
            {
              int index = (int) it->getMetaValue("xl_rank") - 1;

              if (newvec[index].metaValueExists("xl_rank"))
              {
                LOG_ERROR << "ERROR: At least two hits with the same rank within the same spectrum" << endl;
                throw std::exception();
              }
              newvec[index] = *it;
            }
            this->csms_.push_back(newvec);
            */
          this->csms_.push_back(this->current_spectrum_search);

          if(this->cum_hits_ != NULL)
          {
            this->cum_hits_->push_back(this->n_hits_);
          }
        }
        this->current_spectrum_search.clear();
      }
      else if (tag == "xquest_results")
      {      
        this->metas_.push_back(this->current_meta_);
        this->current_meta_.clearMetaInfo();
      }
    }
    void XQuestResultXMLHandler::startElement(const XMLCh * const, const XMLCh * const, const XMLCh * const qname, const Attributes &attributes)
    {
      String tag = XMLString::transcode(qname);
      // Extract meta information from the xquest_results tag
      if (tag == "xquest_results")
      {
      
        /*
        for(XMLSize_t i = 0; i < attributes.getLength(); ++i)
        {
            this->current_meta_.setMetaValue(XMLString::transcode(attributes.getQName(i)),
                                   DataValue(XMLString::transcode(attributes.getValue(i))));
        }
        */
        // Set the search parameters 
        ProteinIdentification::SearchParameters search_params;
        search_params.digestion_enzyme = *this->enzymes_db->getEnzyme(XQuestResultXMLHandler::enzymes.at( this->attributeAsInt_(attributes, "enzyme_num")));
        search_params.missed_cleavages = this->attributeAsInt_(attributes, "missed_cleavages");
        search_params.db = this->attributeAsString_(attributes, "database");
        search_params.precursor_mass_tolerance = this->attributeAsDouble_(attributes, "ms1tolerance");
        String tolerancemeasure = this->attributeAsString_(attributes, "tolerancemeasure");
        search_params.precursor_mass_tolerance_ppm = tolerancemeasure == "ppm";
        
        
        this->prot_ids_[0].setSearchParameters(search_params);  
      }
      else if (tag == "spectrum_search")
      {
        // TODO Store information of spectrum search
      }
      else if (tag == "search_hit")
      {
          // New CrossLinkSpectrumMatchEntry
          this->n_hits_++;
          PeptideIdentification peptide_identification;
          PeptideHit peptide_hit_alpha;
          vector<PeptideHit> peptide_hits;
          // XL Type, determined by "type"
          String xlink_type_string = this->attributeAsString_(attributes, "type");
          String prot1_string = this->attributeAsString_(attributes, "prot1");

          // Decide if decoy for alpha
          DataValue target_decoy = DataValue(prot1_string.hasSubstring("decoy") ? "decoy" : "target");
          peptide_identification.setMetaValue("target_decoy", target_decoy);
          peptide_hit_alpha.setMetaValue("target_decoy", target_decoy);


          // Get Attributes of Peptide Identification
          this->peptide_id_meta_values["OpenXQuest:id"] = DataValue(this->attributeAsString_(attributes, "id"));
          this->peptide_id_meta_values["OpenXQuest:xlinkermass"] = DataValue(this->attributeAsDouble_(attributes, "xlinkermass"));
          this->peptide_id_meta_values["OpenXQuest:wTIC"] = DataValue(this->attributeAsDouble_(attributes, "wTIC"));
          this->peptide_id_meta_values["OpenXQuest:percTIC"] = DataValue(this->attributeAsDouble_(attributes, "TIC"));
          this->peptide_id_meta_values["xl_rank"] = DataValue(this->attributeAsInt_(attributes, "search_hit_rank"));
          this->peptide_id_meta_values["OpenXQuest:intsum"] = DataValue(this->attributeAsDouble_(attributes, "intsum")/100);
          this->peptide_id_meta_values["OpenXQuest:match-odds"] = DataValue(this->attributeAsDouble_(attributes, "match_odds"));
          this->peptide_id_meta_values["OpenXQuest:score"] = DataValue(this->attributeAsDouble_(attributes, "score"));
          this->peptide_id_meta_values["OpenXQuest:error_rel"] = DataValue(this->attributeAsDouble_(attributes, "error_rel"));
          this->peptide_id_meta_values["OpenXQuest:structure"] = DataValue(this->attributeAsString_(attributes, "structure"));

          assert(this->peptide_id_meta_values["OpenXQuest:id"] != DataValue::EMPTY);
          assert(this->peptide_id_meta_values["OpenXQuest:xlinkermass"] != DataValue::EMPTY);
          assert(this->peptide_id_meta_values["OpenXQuest:wTIC"]  != DataValue::EMPTY);
          assert(this->peptide_id_meta_values["OpenXQuest:percTIC"] != DataValue::EMPTY);
          assert(this->peptide_id_meta_values["OpenXQuest:intsum"] != DataValue::EMPTY);
          assert(this->peptide_id_meta_values["OpenXQuest:match-odds"] != DataValue::EMPTY);
          assert(this->peptide_id_meta_values["xl_rank"] != DataValue::EMPTY);
          assert(this->peptide_id_meta_values["OpenXQuest:score"] != DataValue::EMPTY);
          assert(this->peptide_id_meta_values["OpenXQuest:error_rel"] != DataValue::EMPTY);
          assert(this->peptide_id_meta_values["OpenXQuest:structure"] != DataValue::EMPTY);

          this->add_meta_values(peptide_identification);

          // Store common attributes in Peptide Identification
          // If requested, also write to the peptide_hit_alpha
          if (this->load_to_peptideHit_)
          {
              this->add_meta_values(peptide_hit_alpha);
          }

          // Store specific stuff for peptide hit alpha
          peptide_hit_alpha.setMetaValue("OpenXQuest:num_of_matched_ions",
                                         DataValue(this->attributeAsInt_(attributes, "num_of_matched_ions_alpha")));
          peptide_hit_alpha.setMetaValue("OpenXQuest:prot", DataValue(prot1_string));
          
          // Set peptide Evidences for Alpha (need one for each accession in the prot1_string)
          this->setPeptideEvidence_(prot1_string, peptide_hit_alpha);
          
          // Switch on Cross-link type
          if (xlink_type_string == "xlink")
          {
              PeptideHit peptide_hit_beta;
              // If requested, also write to the peptide_hit_beta
              if (this->load_to_peptideHit_)
              {
                this->add_meta_values(peptide_hit_beta);
              }
              // Set xl_type
              this->setMetaValue("xl_type", DataValue("cross-link"), peptide_identification, peptide_hit_alpha, peptide_hit_beta);

              // Set xl positions, depends on xl_type
              std::pair<SignedSize, SignedSize> positions;
              this->getLinkPosition_(attributes, positions);
              peptide_hit_alpha.setMetaValue("xl_pos", DataValue(positions.first));
              peptide_hit_beta.setMetaValue("xl_pos", DataValue(positions.second));

             // Protein
              String prot2_string = this->attributeAsString_(attributes, "prot2");

              // Decide if decoy for beta
              if (prot2_string.hasSubstring("decoy"))
              {
                peptide_identification.setMetaValue("target_decoy", DataValue("decoy"));
                peptide_hit_beta.setMetaValue("target_decoy", DataValue("decoy"));              
              }
              else
              {
                peptide_hit_beta.setMetaValue("target_decoy", DataValue("target"));
              }
             
             
              // Set peptide_hit specific stuff
              peptide_hit_beta.setMetaValue("OpenXQuest:num_of_matched_ions",
                                            DataValue(this->attributeAsInt_(attributes, "num_of_matched_ions_beta")));
              peptide_hit_beta.setMetaValue("OpenXQuest:prot", DataValue(prot2_string));
              
              // Set Peptide Evidences for Beta
              this->setPeptideEvidence_(prot2_string, peptide_hit_beta);

              // Determine if protein is intra/inter protein, check all protein ID combinations
              removeSubstring(prot1_string, "reverse_");
              removeSubstring(prot1_string, "decoy_");
              StringList prot1_list;
              StringUtils::split(prot1_string, ",", prot1_list);
              removeSubstring(prot2_string, "reverse_");
              removeSubstring(prot2_string, "decoy_");
              StringList prot2_list;
              StringUtils::split(prot2_string, ",", prot2_list);

              for (StringList::const_iterator it1 = prot1_list.begin(); it1 != prot1_list.end(); it1++)
              {
                for (StringList::const_iterator it2 = prot2_list.begin(); it2 != prot2_list.end(); it2++)
                {
                   this->setMetaValue((*it1 == *it2) ? "OpenXQuest:is_intraprotein" : "OpenXQuest:is_interprotein",
                                       DataValue(), peptide_identification, peptide_hit_alpha, peptide_hit_beta);
                }
              }
              peptide_hits.push_back(peptide_hit_beta);
          }
          else if (xlink_type_string == "intralink")
          {
              // xl type
              this->setMetaValue("xl_type", DataValue("loop-link"),peptide_identification,peptide_hit_alpha);

              // Set xl positions, depends on xl_type
              std::pair<SignedSize, SignedSize> positions;
              this->getLinkPosition_(attributes, positions);
              peptide_hit_alpha.setMetaValue("xl_pos", DataValue(positions.first));
              peptide_hit_alpha.setMetaValue("xl_pos2", DataValue(positions.second));
          }
          else if (xlink_type_string == "monolink")
          {
             // xl_type
             this->setMetaValue("xl_type", DataValue("mono-link"),peptide_identification,peptide_hit_alpha);

             // Set xl positions_depends on xl_type
             peptide_hit_alpha.setMetaValue("xl_pos", DataValue((SignedSize)this->attributeAsInt_(attributes, "xlinkposition")));
          }
          else
          {
            LOG_ERROR << "ERROR: Unsupported Cross-Link type: " << xlink_type_string << endl;
            throw std::exception();
          }

          // Finalize this record
          peptide_hits.push_back(peptide_hit_alpha);
          peptide_identification.setHits(peptide_hits);
          this->peptide_id_meta_values.clear();
          this->current_spectrum_search.push_back(peptide_identification);
      }
    }
    void XQuestResultXMLHandler::characters(const XMLCh * const chars, const XMLSize_t)
    {
    }


    void XQuestResultXMLHandler::writeTo(ostream &os)
    {


    }




  }   // namespace Internal
} // namespace OpenMS
