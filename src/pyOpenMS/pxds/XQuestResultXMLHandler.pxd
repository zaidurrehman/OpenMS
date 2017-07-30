from Types cimport *
from libcpp cimport bool
from libcpp.vector cimport vector as libcpp_vector
from XMLHandler cimport *
from MetaInfoInterface cimport *
from PeptideIdentification cimport *
from ProteinIdentification cimport *
from PeptideHit cimport *
from EnzymesDB cimport *

cdef extern from "<OpenMS/FORMAT/HANDLERS/XQuestResultXMLHandler.h>" namespace "OpenMS::Internal":
    
    cdef cppclass XQuestResultXMLHandler(XMLHandler) :
        # wrap-inherits:
        #  XMLHandler
        XQuestResultXMLHandler(XQuestResultXMLHandler) nogil except + #wrap-ignore
        libcpp_map[ Size, String ] enzymes
        libcpp_map[ String, UInt ] months
        String decoy_string
        XQuestResultXMLHandler(String & filename, libcpp_vector[ libcpp_vector[ PeptideIdentification ] ] & csms, libcpp_vector[ ProteinIdentification ] & prot_ids, Size min_n_ions_per_spectrum, bool load_to_peptideHit_) nogil except +
        # POINTER # void endElement(XMLCh *uri, XMLCh *localname, XMLCh *qname) nogil except +
        # NAMESPACE # # POINTER # void startElement(XMLCh *uri, XMLCh *localname, XMLCh *qname, xercesc::Attributes & attrs) nogil except +
        double getMinScore() nogil except +
        double getMaxScore() nogil except +
        UInt getNumberOfHits() nogil except +

